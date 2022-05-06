///
/// @file  link_list_queue.hpp
/// @brief An unbounded queue implemented with the linked-list.
///

#ifndef SYNC_CELL_LINK_LIST_QUEUE_HPP
#define SYNC_CELL_LINK_LIST_QUEUE_HPP

#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>

#include "shared/link_list_node.hpp"
#include "util/cache_padded.hpp"
#include "util/copy_move_selector.hpp"


namespace sc {

template<typename T, typename Alloc = std::allocator<impl::Node<T>>>
class LinkListQueue
{
    using Node = impl::Node<T>;
    using AllocTrait = std::allocator_traits<Alloc>;

    static_assert(std::is_same_v<Node, typename AllocTrait::value_type>);

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
    using allocator_type = typename AllocTrait::allocator_type;
    using reference = value_type &;
    using const_reference = const value_type &;

    explicit LinkListQueue(const allocator_type &al = allocator_type()) : allocator_(al)
    {
        Node *p = AllocTrait::allocate(allocator_, 1);
        AllocTrait::construct(allocator_, p);   // construct a default empty node
        head_->store(VersionPtr{p, 0});
        tail_->store(p);
    }

    LinkListQueue(const LinkListQueue &) = delete;

    LinkListQueue &operator=(const LinkListQueue &) = delete;

    ~LinkListQueue()
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
        Node *p = AllocTrait::allocate(allocator_, 1);
        AllocTrait::construct(allocator_, p, std::in_place, value);

        enqueue_node(p);
    }

    template<typename = std::enable_if_t<std::is_move_constructible_v<value_type>>>
    void enqueue(value_type &&value)
    {
        Node *p = AllocTrait::allocate(allocator_, 1);
        AllocTrait::construct(allocator_, p, std::in_place, std::move(value));

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
        AllocTrait::destroy(allocator_, node);
        AllocTrait::deallocate(allocator_, node, 1);
    }

    // dequeue direction
    util::CachePadded<std::atomic<VersionPtr>> head_;
    // enqueue direction
    util::CachePadded<std::atomic<Node *>> tail_;

    // allocator
    allocator_type allocator_;
};

}

#endif //SYNC_CELL_LINK_LIST_QUEUE_HPP
