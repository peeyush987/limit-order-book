#pragma once

#include <atomic>
#include <optional>
#include <utility>

template <typename T>
class LockFreeMSQueue
{
private:
    struct Node
    {
        std::atomic<Node*> next{nullptr};
        std::optional<T> data;

        // Used by the consumer to retain old nodes safely.
        Node* retiredNext = nullptr;

        Node() = default;

        explicit Node(T&& value)
            : data(std::in_place, std::move(value))
        {
        }
    };

    // Only the consumer accesses head_.
    Node* head_ = nullptr;

    // Producers coordinate through tail_ and each node's next.
    std::atomic<Node*> tail_{nullptr};

    // Consumed nodes are reclaimed when the queue is destroyed.
    Node* retired_ = nullptr;

public:
    LockFreeMSQueue()
    {
        Node* dummy = new Node();

        head_ = dummy;
        tail_.store(dummy, std::memory_order_relaxed);
    }

    LockFreeMSQueue(const LockFreeMSQueue&) = delete;
    LockFreeMSQueue& operator=(const LockFreeMSQueue&) = delete;

    ~LockFreeMSQueue()
    {
        // First reclaim nodes retired by the consumer.
        Node* node = retired_;

        while (node != nullptr)
        {
            Node* nextRetired = node->retiredNext;
            delete node;
            node = nextRetired;
        }

        // Then reclaim the current queue chain.
        node = head_;

        while (node != nullptr)
        {
            Node* next = node->next.load(
                std::memory_order_relaxed);

            delete node;
            node = next;
        }
    }

    bool tryPush(T&& value)
    {
        // Allocate and initialize the node before publishing it.
        Node* newNode = new Node(std::move(value));

        for (;;)
        {
            Node* tail =
                tail_.load(std::memory_order_acquire);

            Node* next =
                tail->next.load(std::memory_order_acquire);

            // Retry if tail moved while we were examining it.
            if (tail != tail_.load(std::memory_order_acquire))
            {
                continue;
            }

            if (next == nullptr)
            {
                // Successful linking is the enqueue's
                // linearization point.
                if (tail->next.compare_exchange_weak(
                        next,
                        newNode,
                        std::memory_order_release,
                        std::memory_order_relaxed))
                {
                    // Best-effort tail advancement.
                    tail_.compare_exchange_strong(
                        tail,
                        newNode,
                        std::memory_order_release,
                        std::memory_order_relaxed);

                    return true;
                }
            }
            else
            {
                // Help advance a lagging tail.
                tail_.compare_exchange_weak(
                    tail,
                    next,
                    std::memory_order_release,
                    std::memory_order_relaxed);
            }
        }
    }

    std::optional<T> tryPop()
    {
        // Only one consumer may call tryPop().
        Node* oldHead = head_;

        Node* next =
            oldHead->next.load(std::memory_order_acquire);

        if (next == nullptr)
        {
            // No item has been published at the next position.
            return std::nullopt;
        }

        // The next node contains the oldest queued item.
        T value = std::move(*next->data);
        next->data.reset();

        // Advance the consumer's head.
        head_ = next;

        // Retain the old dummy: producers might still have
        // a pointer to it. Do not delete it while running.
        oldHead->retiredNext = retired_;
        retired_ = oldHead;

        return std::optional<T>{std::move(value)};
    }
};