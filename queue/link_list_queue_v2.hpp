///
/// @file  link_list_queue_v2.hpp
/// @brief
///

#ifndef SYNC_CELL_LINK_LIST_QUEUE_V2_HPP
#define SYNC_CELL_LINK_LIST_QUEUE_V2_HPP

#include <atomic>
#include <memory>
#include <optional>

#include "util/cache_padded.hpp"


#if __cpp_lib_atomic_shared_ptr

namespace sc::mpmc {

template<typename T>
class LinkListQueueV2
{
    struct Node
    {
        std::atomic<std::shared_ptr<Node>> next;
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

    LinkListQueueV2()
    {
        auto dummy = std::make_shared<Node>();
        (*head_).store(dummy);
        (*tail_).store(dummy);
    }

    ~LinkListQueueV2()
    {
        auto *tail = (*tail_).load(std::memory_order_acquire);
        while (!(*tail_).compare_exchange_weak(
                tail,
                nullptr,
                std::memory_order_acq_rel,
                std::memory_order_release)) { }

        std::shared_ptr<Node> head = (*head_).load(std::memory_order_acquire);
        auto p = head.get();
        while (p != tail) {
            clear();
            head = (*head_).load(std::memory_order_acquire);
            p = head.get();
        }
    }

    [[nodiscard]] bool is_lock_free() const noexcept
    {
        return (*head_).is_lock_free();
    }

    template<typename = std::enable_if_t<std::is_copy_constructible_v<value_type>>>
    void enqueue(const_reference value)
    {
        auto node = std::make_shared<Node>(std::in_place, value);

        enqueue_node(node);
    }

    template<typename = std::enable_if_t<std::is_move_constructible_v<value_type>>>
    void enqueue(value_type &&value)
    {
        auto node = std::make_shared<Node>(std::in_place, std::move(value));

        enqueue_node(node);
    }

    std::optional<value_type> try_dequeue()
    {
        auto ptr = (*head_).load(std::memory_order_acquire);
        std::shared_ptr<Node> next;
        do {
            next = ptr->next.load(std::memory_order_acquire);
            if (!next) {
                return {};
            }
        } while (!(*head_).compare_exchange_weak(ptr, next, std::memory_order_acq_rel, std::memory_order_release));

        return std::move(next->value);
    }

    void clear()
    {
        while (try_dequeue()) { }
    }

private:
    void enqueue_node(const std::shared_ptr<Node> &node)
    {
        Node *queue_tail = (*tail_).load(std::memory_order_acquire);
        do {
            if (queue_tail == nullptr) {
                // queue released.
                return;
            }
        } while (!(*tail_).compare_exchange_weak(
                queue_tail,
                node,
                std::memory_order_acq_rel,
                std::memory_order_release));

        queue_tail->next.store(node, std::memory_order_release);
    }

    util::CachePadded<std::atomic<std::shared_ptr<Node>>> head_;
    util::CachePadded<std::atomic<Node *>> tail_;
};

}

#endif

#endif //SYNC_CELL_LINK_LIST_QUEUE_V2_HPP
