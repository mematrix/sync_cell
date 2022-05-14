///
/// @file  blocking_queue_test.cpp
/// @brief Test for sc::BlockingQueue
///

#include "queue/blocking_queue.hpp"
#include "queue/mpmc_array_queue.hpp"

#include "queue_thread_run.hpp"


constexpr uint32_t ProducerCount = 4;
constexpr uint32_t ConsumerCount = 2;

int main()
{
    sc::BlockingQueue<sc::mpmc::ArrayListQueue<Task>> queue;

    queue.enqueue(Task{});
    queue.dequeue();

    std::cout << "hello world" << std::endl;

    return 0;
}
