#pragma once

#include <cstddef>
#include <vector>

struct PoolStats {
    static inline std::size_t freshAllocs = 0;
    static inline std::size_t recycled = 0;
};

template <typename T>
class PoolAllocator {
public:
    using value_type = T;

    PoolAllocator() = default;

    template <typename U>
    PoolAllocator(const PoolAllocator<U>&) {}


    T* allocate(std::size_t n) {
        if (n != 1) return static_cast<T*>(::operator new(n * sizeof(T)));
        if (!freeList_.empty()) {
            PoolStats::recycled++;
            T* p = freeList_.back();
            freeList_.pop_back();
            return p;
        }
        PoolStats::freshAllocs++;
        return static_cast<T*>(::operator new(sizeof(T)));
    }

    void deallocate(T* p, std::size_t n)
    {
        if (n != 1) {
            ::operator delete(p);
            return;
        }

        freeList_.push_back(p);
    }

private:
    static inline std::vector<T*> freeList_;
};