///
/// @file  blocking_queue.hpp
/// @brief An adapter queue that adds blocking dequeue operation for the inner
/// non-blocking queue. If the inner queue supports blocking dequeue, the adapter
/// just forwards the call.
///

#ifndef SYNC_CELL_BLOCKING_QUEUE_HPP
#define SYNC_CELL_BLOCKING_QUEUE_HPP

#include <condition_variable>
#include <mutex>
#include <type_traits>


namespace sc {

template<typename Queue>
class BlockingQueue
{
    template<typename>
    inline static constexpr bool dependent_false_v = false;

public:
    using value_type = typename Queue::value_type;
    using reference = typename Queue::reference;
    using const_reference = typename Queue::reference;

    template<typename... Args>
    explicit BlockingQueue(Args &&... args) noexcept(std::is_nothrow_constructible_v<Queue, Args...>)
            : queue_(std::forward<Args>(args)...)
    {
    }

    BlockingQueue(const BlockingQueue &) noexcept(std::is_nothrow_copy_constructible_v<Queue>) = default;

    BlockingQueue(BlockingQueue &&) noexcept(std::is_nothrow_move_assignable_v<Queue>) = default;

    BlockingQueue &operator=(const BlockingQueue &) noexcept(std::is_nothrow_copy_assignable_v<Queue>) = default;

    BlockingQueue &operator=(BlockingQueue &&) noexcept(std::is_nothrow_move_assignable_v<Queue>) = default;

    template<typename V>
    void enqueue(V &&v)
    {
        if constexpr(std::is_same_v<value_type, std::remove_cvref_t<V>>) {
            queue_.enqueue(std::forward<V>(v));
        } else if constexpr(std::is_constructible_v<V, value_type>) {
            queue_.enqueue(value_type(std::forward<V>(v)));
        } else {
            static_assert(dependent_false_v<V>, "Type is not expected.");
        }

        cond_var_.notify_all();
    }

    auto try_dequeue()
    {
        return queue_.try_dequeue();
    }

    value_type dequeue()
    {
        static_assert(std::is_convertible_v<decltype(*(queue_.try_dequeue())), value_type>);

        while (true) {
            auto v = queue_.try_dequeue();
            if (v) {
                return *std::move(v);
            }

            std::unique_lock lock(mtx_);
            cond_var_.wait(lock);
        }
    }

private:
    Queue queue_;

    std::mutex mtx_;
    std::condition_variable cond_var_;
};

}

#endif //SYNC_CELL_BLOCKING_QUEUE_HPP
