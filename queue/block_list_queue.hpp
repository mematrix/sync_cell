///
/// @file  block_list_queue.hpp
/// @brief An unbounded mpmc queue. Original rust implement:
/// [https://github.com/crossbeam-rs/crossbeam/blob/master/crossbeam-deque/src/deque.rs](Injector).
///

#ifndef SYNC_CELL_BLOCK_LIST_QUEUE_HPP
#define SYNC_CELL_BLOCK_LIST_QUEUE_HPP

#include <atomic>
#include <memory>
#include <optional>

#include "util/back_off.hpp"
#include "util/cache_padded.hpp"


namespace sc::mpmc {

/// @brief A FIFO queue that can be shared among multiple threads. It is lock-free, but sometimes
/// it may wait for another thread to complete progress by using the @c YIELD or @c PAUSE instruction
/// and the current thread may yield by giving up the time slice to the OS scheduler.
/// @tparam T The value type.
template<typename T>
class BlockListQueue
{
    // Bits indicating the state of a slot:
    // If a task has been written into the slot, 'Write' is set.
    static constexpr uint32_t Write = 1u << 0;
    // If a task has been read from the slot, 'Read' is set.
    static constexpr uint32_t Read = 1u << 1;
    // If the block is being destroyed, 'Destroy' is set.
    static constexpr uint32_t Destroy = 1u << 2;

    /// @brief Each block covers one 'Lap' of indices.
    static constexpr uint32_t Lap = 64;
    /// @brief The maximum number of values o block can hold.
    static constexpr uint32_t BlockCap = Lap - 1;

    /// @brief How many lower bits are reserved for metadata.
    static constexpr uint32_t Shift = 1;
    /// @brief Metadata: Indicates that the blocks is not the last one.
    static constexpr uint32_t HasNext = 1;

    /// @brief A slot in a block.
    struct Slot
    {
        /// @brief The value saved.
        std::optional<T> value;
        /// @brief The state of the slot.
        std::atomic<uint32_t> state{0};

        constexpr Slot() noexcept = default;

        /// @brief Waits until a task is written into the slot.
        void wait_write() const noexcept
        {
            util::Backoff backoff;
            while ((state.load(std::memory_order_acquire) & Write) == 0) {
                backoff.snooze();
            }
        }
    };

    /// @brief A block in a linked list.
    ///
    /// Each block in the list can hold up to @c BlockCap values.
    struct Block
    {
        /// @brief The next block in the linked list.
        std::atomic<Block *> next{nullptr};
        /// @brief Slots for values.
        Slot slots[BlockCap];

        constexpr Block() noexcept = default;

        /// @brief Waits until the next pointer is set.
        /// @return The next pointer value.
        Block *wait_next() const noexcept
        {
            util::Backoff backoff;
            while (true) {
                auto *n = next.load(std::memory_order_acquire);
                if (n) {
                    return n;
                }
                backoff.snooze();
            }
        }
    };

    using AllocTrait = std::allocator_traits<std::allocator<Block>>;

    /// @brief Default size of the object cache pool.
    static constexpr size_t DefaultPoolSize = 4;

    /// @brief An object cache pool to improve the performance of @c Block object allocation
    /// on a concurrent @c BlockListQueue::enqueue call.
    /// @tparam N The pool size.
    template<size_t N = DefaultPoolSize>
    class BlockCachePool
    {
    public:
        constexpr BlockCachePool() noexcept = default;

        Block *alloc()
        {
            Block *ret = nullptr;
            for (int i = 0; i < N; ++i) {
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

            AllocTrait::construct(allocator_, ret);
            return ret;
        }

        void dealloc(Block *block)
        {
            AllocTrait::destroy(allocator_, block);

//            std::atomic_thread_fence(std::memory_order_release);
            for (int i = 0; i < N; ++i) {
                auto *p = alloc_cache_[i].load(std::memory_order_relaxed);
                if (p == nullptr &&
                    alloc_cache_[i].compare_exchange_strong(
                            p, block,
                            std::memory_order_relaxed,
                            std::memory_order_relaxed)) {
                    return;
                }
            }

            AllocTrait::deallocate(allocator_, block, 1);
        }

        ~BlockCachePool()
        {
            for (int i = 0; i < N; ++i) {
                auto *p = alloc_cache_[i].load(std::memory_order_relaxed);
                if (p != nullptr &&
                    alloc_cache_[i].compare_exchange_strong(
                            p, nullptr,
                            std::memory_order_relaxed,
                            std::memory_order_relaxed)) {
                    AllocTrait::deallocate(allocator_, p, 1);
                }
            }
//            std::atomic_thread_fence(std::memory_order_acquire);
        }

    private:
        std::atomic<Block *> alloc_cache_[N];
        std::allocator<Block> allocator_;
    };

    template<size_t N = DefaultPoolSize>
    struct PoolBlockDeleter
    {
        BlockCachePool<N> *pool;

        void operator()(Block *block)
        {
            pool->dealloc(block);
        }
    };

    /// @brief Creates an empty block managed by a @c std::unique_ptr, which will auto release
    /// the new block to the 'pool' if the block does not been added to the linked list.
    /// @param pool A @c Block object cache pool.
    static std::unique_ptr<Block, PoolBlockDeleter<>> new_block(BlockCachePool<> &pool)
    {
        auto *b = pool.alloc();
        return {b, {&pool}};
    }

    /// @brief Sets the @c Destroy bits in slots starting from 'count' and destroy the block.
    static void destroy_block(Block *self, uint32_t count, BlockCachePool<> &pool)
    {
        // It is not necessary to set the 'Destroy' bit in the last slot because that slot has
        // begun destruction of the block.
        for (int32_t i = (int32_t)count - 1; i >= 0; --i) {
            auto &slot = self->slots[i];

            // Mark the 'Destroy' bit if a thread is still using the slot.
            if ((slot.state.load(std::memory_order_acquire) & Read) == 0 &&
                (slot.state.fetch_or(Destroy, std::memory_order_acq_rel) & Read) == 0) {
                // If a thread is still using the slot, it will continue destruction of the block.
                return;
            }
        }

        // No thread is using the block, now it is safe to destroy it.
        pool.dealloc(self);
    }

    /// @brief A position in a queue.
    struct Position
    {
        /// @brief The index in the queue.
        std::atomic<size_t> index;
        /// @brief The block in the linked list.
        std::atomic<Block *> block;
    };

public:
    using value_type = T;
    using reference = value_type &;
    using const_reference = const value_type &;

    /// @brief Creates a new queue.
    BlockListQueue()
    {
        auto *block = pool_.alloc();
        (*head_).block.store(block);
        (*head_).index.store(0);
        (*tail_).block.store(block);
        (*tail_).index.store(0);
    }

    BlockListQueue(const BlockListQueue &) = delete;

    BlockListQueue &operator=(const BlockListQueue &) = delete;

    template<typename = std::enable_if_t<std::is_copy_constructible_v<value_type>>>
    void enqueue(const_reference value)
    {
        enqueue_value([&value](std::optional<value_type> &o) {
            o.template emplace(value);
        });
    }

    template<typename = std::enable_if_t<std::is_move_constructible_v<value_type>>>
    void enqueue(value_type &&value)
    {
        enqueue_value([&value](std::optional<value_type> &o) {
            o.template emplace(std::move(value));
        });
    }

private:
    template<typename Func>
    void enqueue_value(Func value_set)
    {
        //
    }

    /// @brief The head of the queue.
    util::CachePadded<Position> head_;
    /// @brief The tail of the queue.
    util::CachePadded<Position> tail_;

    /// @brief Default cache pool.
    BlockCachePool<> pool_;
};

}

#endif //SYNC_CELL_BLOCK_LIST_QUEUE_HPP
