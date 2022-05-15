///
/// @file  blocking_queue_test.cpp
/// @brief Test for sc::BlockingQueue
///

#include "queue/blocking_queue.hpp"
#include "queue/mpmc_array_queue.hpp"

#include "queue_thread_run.hpp"


constexpr uint32_t ProducerCount = 4;
constexpr uint32_t ConsumerCount = 2;

struct ObjHasDequeue
{
    char value = 0;

    char &dequeue() { return value; }
};

struct ObjNoDequeue
{
};

int main()
{
    std::cout << std::boolalpha;
    std::cout << "ObjHasDequeue check: " << sc::impl::HasDequeue<ObjHasDequeue>::value << std::endl;
    std::cout << "ObjNoDequeue check: " << sc::impl::HasDequeue<ObjNoDequeue>::value << std::endl;

    sc::BlockingQueue<sc::mpmc::ArrayListQueue<Task>> queue;

    queue.enqueue(Task{});
    queue.dequeue();

    std::cout << "hello world" << std::endl;

    return 0;
}
