///
/// @file  mpmc_list_queue.hpp
/// @brief An unbounded mpmc queue implemented with the linked-list.
///

#ifndef SYNC_CELL_MPMC_LIST_QUEUE_HPP
#define SYNC_CELL_MPMC_LIST_QUEUE_HPP

#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>

#include "shared/compiler_workaround.hpp"
#include "shared/object_cache_pool.hpp"
#include "util/cache_padded.hpp"
#include "util/copy_move_selector.hpp"


namespace sc::mpmc {

template<typename T>
class LinkedListQueue
{
    struct Node
    {
        std::atomic<Node *> next{nullptr};
        std::optional<T> value;

        constexpr Node() = default;

        template<typename... Args>
        explicit Node(std::in_place_t, Args &&... args) : value(std::in_place, std::forward<Args>(args)...)
        {
            static_assert(std::is_constructible_v<T, Args &&...>);
        }
    };

    /* default size for cache pool */
    static constexpr uint32_t DefaultPoolSize = 0;

    static constexpr size_t PointerSize = sizeof(void *);

    /// @brief Paired a pointer with a version info to avoid the ABA problem.
    /// Whenever we change the pointer, the version is incremented.
    struct alignas(2 * PointerSize) VersionPtr
    {
        Node *ptr;
        uintptr_t version;
    };
    static_assert(alignof(VersionPtr) == 2 * PointerSize);
    static_assert(sizeof(VersionPtr) == 2 * PointerSize);

public:
    using value_type = T;
    using reference = value_type &;
    using const_reference = const value_type &;

    LinkedListQueue()
    {
        Node *p = pool_.TEMPLATE_CALL alloc();   // construct a default empty node
        head_->store(VersionPtr{p, 0});
        tail_->store(p);
    }

    LinkedListQueue(const LinkedListQueue &) = delete;

    LinkedListQueue &operator=(const LinkedListQueue &) = delete;

    ~LinkedListQueue()
    {
        auto *tail = tail_->load(std::memory_order_acquire);
        while (!tail_->compare_exchange_weak(
                tail,
                nullptr,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) { }

        auto head = head_->load(std::memory_order_acquire);
        while (head.ptr != tail) {
            clear();
            head = head_->load(std::memory_order_acquire);
        }

        release_node(tail);
    }

    [[nodiscard]] bool is_lock_free() const noexcept
    {
        return head_->is_lock_free() && tail_->is_lock_free();
    }

    // todo: try-pop & pop(wait, spin first, sleep if spin long time)
    template<typename = std::enable_if_t<std::is_copy_constructible_v<value_type>>>
    void enqueue(const_reference value)
    {
        Node *p = pool_.TEMPLATE_CALL alloc(std::in_place, value);

        enqueue_node(p);
    }

    template<typename = std::enable_if_t<std::is_move_constructible_v<value_type>>>
    void enqueue(value_type &&value)
    {
        Node *p = pool_.TEMPLATE_CALL alloc(std::in_place, std::move(value));

        enqueue_node(p);
    }

    std::optional<value_type> try_dequeue()
    {
        auto ptr = head_->load(std::memory_order_acquire);
        ptr.version = 0;
        while (!head_->compare_exchange_weak(
                ptr,
                VersionPtr{ptr.ptr, 1},
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            ptr.version = 0;
        }

        auto *next = ptr.ptr->next.load(std::memory_order_acquire);
        head_->store(VersionPtr{next == nullptr ? ptr.ptr : next, 0}, std::memory_order_release);

        if (next == nullptr) {
            return {};
        }

        std::optional<value_type> ret(util::cast_ctor_ref(next->value));
        release_node(ptr.ptr);
        return ret;
    }

    void clear()
    {
        while (try_dequeue()) { }
    }

private:
    void enqueue_node(Node *node)
    {
        Node *queue_tail = tail_->load(std::memory_order_acquire);
        do {
            if (queue_tail == nullptr) {
                // queue released. destroy the node
                release_node(node);

                return;
            }
        } while (!tail_->compare_exchange_weak(
                queue_tail,
                node,
                std::memory_order_acq_rel,
                std::memory_order_acquire));

        // now, the queue.tail_ points to the 'node', and we fetched the last tail saved
        // in 'queue_tail' variable.
        //
        // Memory SAFETY:
        // before we set the 'next' of 'queue_tail' (the last tail node), memory pointed
        // by 'queue_tail' will never be released, because when 'queue_tail->next' is
        // null, the queue.head_ will never forward, so the node that queue.head_ pointed
        // will always keep alive. BTW, only when queue.head_ == queue.tail_, the queue
        // can be destroyed.
        //
        // ABA SAFETY:
        // we only need a pointer value but not the value that pointer point to, so even
        // the queue.tail_ changed between we CAS it, as the pointer value is same, which
        // means that current queue tail (whose next value is null) is in the same memory
        // position, so we can deref the pointer safety and all thing is right: we update
        // the "queue tail" to the current node.

        queue_tail->next.store(node, std::memory_order_release);
    }

    void release_node(Node *node)
    {
        pool_.dealloc(node);
    }

    // dequeue direction
    util::CachePadded<std::atomic<VersionPtr>> head_;
    // enqueue direction
    util::CachePadded<std::atomic<Node *>> tail_;

    // allocator
    ObjectCachePool<Node, DefaultPoolSize> pool_;
};

}

#endif //SYNC_CELL_MPMC_LIST_QUEUE_HPP
