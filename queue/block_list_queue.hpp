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
#include "util/copy_move_selector.hpp"


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

    using PoolBlockPtr = std::unique_ptr<Block, PoolBlockDeleter<>>;

    /// @brief Creates an empty block managed by a @c std::unique_ptr, which will auto release
    /// the new block to the 'pool' if the block does not been added to the linked list.
    /// @param pool A @c Block object cache pool.
    static PoolBlockPtr new_block(BlockCachePool<> &pool)
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
#if defined(_MSC_VER)
            o.emplace(value);
#else
            o.template emplace(value);
#endif
        });
    }

    template<typename = std::enable_if_t<std::is_move_constructible_v<value_type>>>
    void enqueue(value_type &&value)
    {
        enqueue_value([&value](std::optional<value_type> &o) {
#if defined(_MSC_VER)
            o.emplace(std::move(value));
#else
            o.template emplace(std::move(value));
#endif
        });
    }

    /// @brief Try dequeue an item from the queue.
    /// @return If success, the optional takes the dequeue value, otherwise the optional is empty.
    std::optional<value_type> try_dequeue()
    {
        util::Backoff backoff;
        size_t head;
        Block *block;
        size_t offset;

        while (true) {
            head = (*head_).index.load(std::memory_order_acquire);
            block = (*head_).block.load(std::memory_order_acquire);

            // Calculate the offset of the index into the block.
            offset = (head >> Shift) % Lap;

            // If we reached the end of the block
            if (offset == BlockCap) {
                backoff.snooze();
            } else {
                break;
            }
        }

        auto new_head = head + (1u << Shift);
        if ((new_head & HasNext) == 0) {
            std::atomic_thread_fence(std::memory_order_seq_cst);
            auto tail = (*tail_).index.load(std::memory_order_relaxed);

            // If the tail equals the head, that means the queue is empty.
            if ((head >> Shift) == (tail >> Shift)) {
                return {};
            }

            // If head and tail are not in the same block, set 'HasNext' in head.
            if ((head >> Shift) / Lap != (tail >> Shift) / Lap) {
                new_head |= HasNext;
            }
        }

        // Try moving the head index forward.
        if (!(*head_).index.compare_exchange_weak(
                head, new_head,
                std::memory_order_seq_cst,
                std::memory_order_relaxed)) {
            return {};
        }

        // If we've reached the end of the block, move to the next one.
        if (offset + 1 == BlockCap) {
            auto next = block->wait_next();
            auto next_index = (new_head & ~HasNext) + (1u << Shift);
            if (next->next.load(std::memory_order_relaxed) != nullptr) {
                next_index |= HasNext;
            }

            (*head_).block.store(next, std::memory_order_release);
            (*head_).index.store(next_index, std::memory_order_release);
        }

        // Read the slot
        auto &slot = block->slots[offset];
        slot.wait_write();
        std::optional<value_type> ret(util::cast_ctor_ref(slot.value));

        // Destroy the block if we've reached the end, or if another thread wanted to destroy
        // but couldn't because we were busy reading from the slot.
        if ((offset + 1 == BlockCap) ||
            ((slot.state.fetch_or(Read, std::memory_order_acq_rel) & Destroy) != 0)) {
            destroy_block(block, offset, pool_);
        }

        return ret;
    }

private:
    template<typename Func>
    void enqueue_value(Func value_set)
    {
        util::Backoff backoff;
        auto tail = (*tail_).index.load(std::memory_order_acquire);
        auto *block = (*tail_).block.load(std::memory_order_acquire);
        PoolBlockPtr next_block;

        while (true) {
            // Calculate the offset of the index into the block.
            auto offset = (tail >> Shift) % Lap;
            // If we reached the end of the block, wait until the next one is installed.
            // Note that because offset is equal to the 'BlockCap', so there must be a
            // thread whose offset is equal to the 'BlockCap - 1', and that thread will
            // set the next block.
            if (offset == BlockCap) {
                backoff.snooze();
                tail = (*tail_).index.load(std::memory_order_acquire);
                block = (*tail_).block.load(std::memory_order_acquire);
                continue;
            }

            // If we're going to have to install the next block, allocate it in advance
            // in order to make the wait for other threads as short as possible.
            if (offset + 1 == BlockCap && !next_block) {
                next_block = new_block(pool_);
            }

            auto new_tail = tail + (1u << Shift);

            // Try advancing the tail forward.
            if ((*tail_).index.compare_exchange_weak(
                    tail, new_tail,
                    std::memory_order_seq_cst,
                    std::memory_order_acquire)) {
                // If we've reached the end of the block, install the next one.
                if (offset + 1 == BlockCap) {
                    // this progress is excluded.
                    auto *b = next_block.release();
                    auto next_index = new_tail + (1u << Shift);

                    (*tail_).block.store(b, std::memory_order_release);
                    (*tail_).index.store(next_index, std::memory_order_release);
                    block->next.store(b, std::memory_order_release);
                }

                // Write the task into the slot.
                auto &slot = block->slots[offset];
                value_set(slot.value);
                slot.state.fetch_or(Write, std::memory_order_release);

                return;
            } else {
                block = (*tail_).block.load(std::memory_order_acquire);
                backoff.spin();
            }
        }
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
