///
/// @file  mpsc_list_queue.hpp
/// @brief An unbounded mpsc queue.
///

#ifndef SYNC_CELL_MPSC_LIST_QUEUE_HPP
#define SYNC_CELL_MPSC_LIST_QUEUE_HPP

#include <atomic>
#include <memory>
#include <optional>

#include "shared/compiler_workaround.hpp"
#include "shared/object_cache_pool.hpp"
#include "util/cache_padded.hpp"
#include "util/copy_move_selector.hpp"


namespace sc::mpsc {

template<typename T, uint32_t PoolSize = 0>
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

public:
    using value_type = T;
    using reference = value_type &;
    using const_reference = const value_type &;

    static constexpr uint32_t PoolCacheSize = PoolSize;

    LinkedListQueue()
    {
        Node *p = pool_.TEMPLATE_CALL alloc();   // construct a default empty node
        head_ = p;
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

        auto *head = head_;
        while (head != tail) {
            clear();
            head = head_;
        }

        release_node(tail);
    }

    [[nodiscard]] bool is_lock_free() const noexcept
    {
        return tail_->is_lock_free();
    }

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

    /// @brief Try dequeue an item.
    /// @note Do not call this method in multiple threads, even if the calls are not concurrent.
    std::optional<value_type> try_dequeue()
    {
        auto *h = head_;
        auto *next = h->next.load(std::memory_order_acquire);
        if (next == nullptr) {
            return {};
        }

        head_ = next;
        release_node(h);

        return util::cast_ctor_ref(next->value);
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

    // enqueue direction
    util::CachePadded<std::atomic<Node *>> tail_;
    // dequeue direction
    Node *head_;

    // allocator
    ObjectCachePool<Node, PoolSize> pool_;
};

}

#endif //SYNC_CELL_MPSC_LIST_QUEUE_HPP
