///
/// @file  link_list_queue.hpp
/// @brief An unbounded queue implemented with the linked-list.
///

#ifndef SYNC_CELL_LINK_LIST_QUEUE_HPP
#define SYNC_CELL_LINK_LIST_QUEUE_HPP

#include <atomic>
#include <memory>
#include <new>  // std::hardware_destructive_interference_size
#include <optional>
#include <type_traits>


namespace sc {

namespace impl {

template<typename T>
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

}

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

    /// @brief The temporary list to stash the enqueue data node. The enqueue process
    /// is completed by two stages: first add the new node to the stash list, then
    /// add the stash list to the tail of the queue.
    ///
    /// The algorithm steps of enqueuing a node(ptr): \n
    /// 1. try use CAS to set StashList.head=node & StashList.tail=null; \n
    /// 2. if step 1 success, then the node is StashList's head; goto step 5; \n
    /// 3. if step 1 failed, the StashList has been updated by other thread, loop the
    /// operations until the CAS success: save the current StashList value(returned by
    /// CAS operation) to TempList, use CAS try to update set StashList.tail=node and
    /// keep StashList.head unchanged; \n
    /// 4. add node to list: TempList.tail->next.store(node); if tail is nullptr, use
    /// TempList.head; \n
    /// 5. if StashList.head is current node(the step 1 success), then the current
    /// thread is responsible to add the StashList to the queue: use CAS to update set
    /// StashList.head=null & StashList.tail=null until the CAS success, then update
    /// the tail pointer of queue: \n
    /// <code>
    /// let QueueTail = (tail ptr of queue); <br>
    /// let TempList = (step5 CAS return value); <br>
    /// let Tail = if TempList.tail == null { TempList.head } else { TempList.tail }; <br>
    /// while (!CAS(Queue$Tail, QueueTail, Tail)) {} // Queue$Tail is the var of queue; <br>
    /// QueueTail->next.store(TempList.head); // concat the list to queue tail; <br>
    /// </code>
    ///
    /// @note There is a special hardcode value (head=0, tail=PointerSize), which indicates
    /// that stop enqueue any new node (used when the Queue destroy).
    struct alignas(2 * PointerSize) StashList
    {
        Node *head;
        Node *tail;
    };
    static_assert(alignof(StashList) == 2 * PointerSize);
    static_assert(sizeof(StashList) == 2 * PointerSize);

public:
    using value_type = T;
    using allocator_type = typename AllocTrait::allocator_type;
    using size_type = typename AllocTrait::size_type;
    using reference = value_type &;
    using const_reference = const value_type &;

    explicit LinkListQueue(const allocator_type &al = allocator_type()) : allocator_(al)
    {
        Node *p = AllocTrait::allocate(allocator_, 1);
        AllocTrait::construct(allocator_, p);   // construct a default empty node
        head_.store(VersionPtr{p, 0});
        tail_.store(p);
        stash_list_.store(StashList{nullptr, nullptr});
    }

    ~LinkListQueue()
    {
        auto *tail = tail_.load(std::memory_order_acquire);
        while (!tail_.compare_exchange_weak(
                tail,
                nullptr,
                std::memory_order_acq_rel,
                std::memory_order_release)) { }

        StashList expected{nullptr, nullptr};
        const StashList stop_flag{nullptr, (Node *)PointerSize};
        while (!stash_list_.compare_exchange_weak(
                expected,
                stop_flag,
                std::memory_order_acq_rel,
                std::memory_order_release)) {
            expected.head = nullptr;
            expected.tail = nullptr;
        }

        auto head = head_.load(std::memory_order_acquire);
        while (head.ptr != tail) {
            clear();
            head = head_.load(std::memory_order_acquire);
        }

        release_node(tail);
    }

    [[nodiscard]] bool is_lock_free() const noexcept
    {
        return head_.is_lock_free() && stash_list_.is_lock_free();
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
        auto ptr = head_.load(std::memory_order_acquire);
        ptr.version = 0;
        while (!head_.compare_exchange_weak(
                ptr,
                VersionPtr{ptr.ptr, 1},
                std::memory_order_acq_rel,
                std::memory_order_release)) {
            ptr.version = 0;
        }

        auto *next = ptr.ptr->next.load(std::memory_order_acquire);
        head_.store(VersionPtr{next == nullptr ? ptr.ptr : next, 0}, std::memory_order_release);

        if (next == nullptr) {
            return {};
        }

        std::optional<value_type> ret(std::move(next->value));
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
        StashList temp_list{nullptr, nullptr};
        while (true) {
            if (stash_list_.compare_exchange_strong(
                    temp_list,
                    StashList{node, nullptr},
                    std::memory_order_acq_rel,
                    std::memory_order_release)) {
                // success. 'node' became the stash list head.
                temp_list.head = node;
                break;
            } else {
                // fail. another thread has set the stash list head; or stopped.
                // try to add to the stash list.
                if (add_stash_list(node, temp_list)) {
                    return;
                }

                // add failed, temp_list.head = temp_list.tail = nullptr.
            }
        }

        // current thread set the stash list head.
        // set stash_list to default value to commit the list.
        while (!stash_list_.compare_exchange_weak(
                temp_list,
                StashList{nullptr, nullptr},
                std::memory_order_acq_rel,
                std::memory_order_release)) { }
        // check all list node ready
        if (temp_list.tail != nullptr) {
            auto *h = temp_list.head;
            while (h) {
                h->next.wait(nullptr, std::memory_order_acquire);
                auto *n = h->next.load(std::memory_order_acquire);
                if (n == temp_list.tail) {
                    break;
                }

                h = n;
            }
        }

        // update tail and add to queue tail
        auto *tail = temp_list.tail;
        if (tail == nullptr) {
            tail = temp_list.head;
        }
        Node *queue_tail = tail_.load(std::memory_order_acquire);
        do {
            if (queue_tail == nullptr) {
                // queue released. destroy the stash list
                auto *h = temp_list.head;
                while (h) {
                    auto *tmp = h;
                    h = h->next.load(std::memory_order_relaxed);
                    release_node(tmp);
                }

                return;
            }
        } while (!tail_.compare_exchange_weak(
                queue_tail,
                tail,
                std::memory_order_acq_rel,
                std::memory_order_release));
        queue_tail->next.store(temp_list.head, std::memory_order_release);
    }

    bool add_stash_list(Node *node, StashList &temp_list)
    {
        do {
            if (temp_list.head == nullptr) {
                if (temp_list.tail == nullptr) {
                    // stash list has been committed to the queue. retry need.
                    return false;
                } else if ((uintptr_t)temp_list.tail == PointerSize) {
                    // release node and return.
                    release_node(node);
                    return true;
                }
            }
        } while (!stash_list_.compare_exchange_weak(
                temp_list,
                StashList{temp_list.head, node},
                std::memory_order_acq_rel,
                std::memory_order_release));

        // add node to list tail.
        if (temp_list.tail == nullptr) {
            temp_list.head->next.store(node, std::memory_order_release);
            temp_list.head->next.notify_one();
        } else {
            temp_list.tail->next.store(node, std::memory_order_release);
            temp_list.tail->next.notify_one();
        }

        return true;
    }

    void release_node(Node *node)
    {
        AllocTrait::destroy(allocator_, node);
        AllocTrait::deallocate(allocator_, node, 1);
    }

    // todo: add cache line alignment.
    // dequeue direction
    alignas(64) std::atomic<VersionPtr> head_;
    // enqueue direction
    alignas(64) std::atomic<Node *> tail_;
    // enqueue stash list
    alignas(64) std::atomic<StashList> stash_list_;

    // allocator
    allocator_type allocator_;
};

}

#endif //SYNC_CELL_LINK_LIST_QUEUE_HPP
