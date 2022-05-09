///
/// @file  object_cache_pool.hpp
/// @brief A simple object cache pool to reduce the call count of memory alloc/free.
///

#ifndef SYNC_CELL_OBJECT_CACHE_POOL_HPP
#define SYNC_CELL_OBJECT_CACHE_POOL_HPP

#include <cstdint>
#include <atomic>
#include <memory>
#include <type_traits>


namespace sc {

template<typename T, uint32_t N>
class ObjectCachePool
{
    static_assert(!std::is_void_v<T> && !std::is_reference_v<T>);

    using Alloc = std::allocator<T>;    // using a template param?
    using AllocTrait = std::allocator_traits<Alloc>;

public:
    using pointer_type = T *;

    constexpr ObjectCachePool() noexcept = default;

    ~ObjectCachePool()
    {
        for (uint32_t i = 0; i < N; ++i) {
            auto *p = alloc_cache_[i].load(std::memory_order_relaxed);
            if (p != nullptr &&
                alloc_cache_[i].compare_exchange_strong(
                        p, nullptr,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                AllocTrait::deallocate(allocator_, p, 1);
            }
        }
    }

    template<typename... Args>
    pointer_type alloc(Args &&... args)
    {
        pointer_type ret = nullptr;
        for (uint32_t i = 0; i < N; ++i) {
            auto *p = alloc_cache_[i].load(std::memory_order_relaxed);
            if (p != nullptr &&
                alloc_cache_[i].compare_exchange_strong(
                        p, nullptr,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                ret = p;
                break;
            }
        }

        if (ret == nullptr) {
            ret = AllocTrait::allocate(allocator_, 1);
        }

        AllocTrait::construct(allocator_, ret, std::forward<Args>(args)...);
        return ret;
    }

    void dealloc(pointer_type ptr)
    {
        AllocTrait::destroy(allocator_, ptr);

        for (uint32_t i = 0; i < N; ++i) {
            auto *p = alloc_cache_[i].load(std::memory_order_relaxed);
            if (p == nullptr &&
                alloc_cache_[i].compare_exchange_strong(
                        p, ptr,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                return;
            }
        }

        AllocTrait::deallocate(allocator_, ptr, 1);
    }

private:
    std::atomic<T *> alloc_cache_[N];
    Alloc allocator_;
};

// Specialization for cache size == 0.
template<typename T>
class ObjectCachePool<T, 0>
{
    using Alloc = std::allocator<T>;    // using a template param?
    using AllocTrait = std::allocator_traits<Alloc>;

public:
    using pointer_type = T *;

    constexpr ObjectCachePool() noexcept = default;

    template<typename... Args>
    pointer_type alloc(Args &&... args)
    {
        pointer_type ret = AllocTrait::allocate(allocator_, 1);
        AllocTrait::construct(allocator_, ret, std::forward<Args>(args)...);

        return ret;
    }

    void dealloc(pointer_type ptr)
    {
        AllocTrait::destroy(allocator_, ptr);
        AllocTrait::deallocate(allocator_, ptr, 1);
    }

private:
    Alloc allocator_;
};

}

#endif //SYNC_CELL_OBJECT_CACHE_POOL_HPP
