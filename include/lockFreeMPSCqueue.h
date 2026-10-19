#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

template <typename T, std::size_t Capacity>
class LockFreeMPSCQueue
{
    static_assert(Capacity >= 2,
                  "Queue capacity must be at least 2");

    struct Cell
    {
        std::atomic<std::size_t> sequence{0};
        std::optional<T> data;
    };

    std::array<Cell, Capacity> cells_{};

    // Multiple producers share this position.
    std::atomic<std::size_t> enqueuePos_{0};

    // Only the single consumer accesses this position.
    std::size_t dequeuePos_ = 0;

public:
    LockFreeMPSCQueue()
    {
        for (std::size_t i = 0; i < Capacity; ++i)
        {
            cells_[i].sequence.store(
                i, std::memory_order_relaxed);
        }
    }

    bool tryPush(T&& value)
    {
        Cell* cell = nullptr;

        std::size_t pos =
            enqueuePos_.load(std::memory_order_relaxed);

        for (;;)
        {
            cell = &cells_[pos % Capacity];

            const std::size_t seq =
                cell->sequence.load(std::memory_order_acquire);

            const std::intptr_t difference =
                static_cast<std::intptr_t>(seq) -
                static_cast<std::intptr_t>(pos);

            if (difference == 0)
            {
                if (enqueuePos_.compare_exchange_weak(
                        pos,
                        pos + 1,
                        std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (difference < 0)
            {
                return false;
            }
            else
            {
                pos = enqueuePos_.load(
                    std::memory_order_relaxed);
            }
        }

        // The producer has reserved this slot.
        cell->data.emplace(std::move(value));

        // Publish the completed command.
        cell->sequence.store(
            pos + 1,
            std::memory_order_release);

        return true;
    }

    std::optional<T> tryPop()
    {
        const std::size_t pos = dequeuePos_;
        Cell& cell = cells_[pos % Capacity];

        const std::size_t seq =
            cell.sequence.load(std::memory_order_acquire);

        const std::intptr_t difference =
            static_cast<std::intptr_t>(seq) -
            static_cast<std::intptr_t>(pos + 1);

        // The next command has not been published.
        if (difference != 0)
        {
            return std::nullopt;
        }

        // Move the command out of the slot.
        T value = std::move(*cell.data);

        // Destroy the moved-from value.
        cell.data.reset();

        // Publish that the slot is available for reuse.
        cell.sequence.store(
            pos + Capacity,
            std::memory_order_release);

        // Advance the single consumer's position.
        dequeuePos_ = pos + 1;

        return std::optional<T>{std::move(value)};
    }
};