///
/// @file  mpmc_array_queue.hpp
/// @brief An unbounded mpmc queue. Original rust implement:
/// [Injector](https://github.com/crossbeam-rs/crossbeam/blob/master/crossbeam-deque/src/deque.rs).
///

#ifndef SYNC_CELL_MPMC_ARRAY_QUEUE_HPP
#define SYNC_CELL_MPMC_ARRAY_QUEUE_HPP

#include "deque/injector.hpp"


namespace sc::mpmc {

/// @brief A FIFO queue that can be shared among multiple threads. It is lock-free, but sometimes
/// it may wait for another thread to complete progress by using the @c YIELD or @c PAUSE instruction
/// and the current thread may yield by giving up the time slice to the OS scheduler.
/// @tparam T The value type.
template<typename T>
class ArrayListQueue
{
public:
    using value_type = T;

    ArrayListQueue() = default;
    ~ArrayListQueue() = default;

    template<typename = std::enable_if_t<std::is_copy_constructible_v<value_type>>>
    void enqueue(const value_type &value)
    {
        injector_.push(value);
    }

    template<typename = std::enable_if_t<std::is_move_constructible_v<value_type>>>
    void enqueue(value_type &&value)
    {
        injector_.push(std::move(value));
    }

    /// @brief Try dequeue an item from the queue.
    /// @return If success, the optional takes the dequeue value, otherwise the optional is empty.
    std::optional<value_type> try_dequeue()
    {
        return injector_.steal();
    }

private:
    sc::scheduler::Injector<T> injector_;
};

}

#endif //SYNC_CELL_MPMC_ARRAY_QUEUE_HPP
