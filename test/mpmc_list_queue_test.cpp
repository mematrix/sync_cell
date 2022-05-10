///
/// @file  mpmc_list_queue_test.cpp
/// @brief Test for 'sc::mpmc::LinkedListQueue' and 'sc::mpmc::LinkedListQueueV2'.
///

#include "queue/mpmc_list_queue.hpp"
#include "queue/mpmc_list_queue_v2.hpp"

#include <cstring>

#include "queue_thread_run.hpp"


constexpr uint32_t ProducerCount = 4;
constexpr uint32_t ConsumerCount = 2;

template<typename Queue>
void run()
{
    Queue task_queue;
    std::atomic_flag barrier = ATOMIC_FLAG_INIT;

    std::cout << "Queue is lock free: " << std::boolalpha <<
              task_queue.is_lock_free() << std::endl;

    std::vector<std::thread> produce_threads;
    produce_threads.reserve(ProducerCount);
    for (uint32_t i = 0; i < ProducerCount; ++i) {
        produce_threads.emplace_back(
                produce<Queue>,
                std::ref(task_queue), std::ref(barrier));
    }

    constexpr uint64_t Total = ProducerCount * LoopCount;
    constexpr uint64_t ConsumerResultSize = Total / ConsumerCount;
    static_assert(ConsumerResultSize * ConsumerCount == Total);

    std::vector<std::thread> consume_threads;
    consume_threads.reserve(ConsumerCount);
    std::vector<std::vector<Task>> result(ConsumerCount);
    for (auto &r: result) {
        r.reserve(ConsumerResultSize);
    }
    for (uint32_t i = 0; i < ConsumerCount; ++i) {
        consume_threads.emplace_back(
                consume<Queue>,
                std::ref(task_queue), std::ref(barrier),
                std::ref(result[i]), ConsumerResultSize);
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    // begin
    barrier.test_and_set();
    barrier.notify_all();

    for (auto &t: produce_threads) {
        t.join();
    }
    for (auto &t: consume_threads) {
        t.join();
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "-v2") == 0) {
#if __cpp_lib_atomic_shared_ptr
        run<sc::mpmc::LinkedListQueueV2<Task>>();
#else
        std::cout << "Error: Built the test without cpp atomic_shared_ptr support." << std::endl;
#endif
    } else {
        run<sc::mpmc::LinkedListQueue<Task>>();
    }

    std::cout << "hello world" << std::endl;

    return 0;
}
