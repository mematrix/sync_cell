///
/// @file  mpmc_list_queue_test.cpp
/// @brief Test for 'sc::mpmc::LinkedListQueue' and 'sc::mpmc::LinkedListQueueV2'.
///

#include "queue/mpmc_list_queue.hpp"
#include "queue/mpmc_list_queue_v2.hpp"

#include <cstring>

#include "queue_thread_run.hpp"


template<typename Queue>
void run()
{
    Queue task_queue;
    std::atomic_flag barrier = ATOMIC_FLAG_INIT;

    std::cout << "Queue is lock free: " << std::boolalpha <<
              task_queue.is_lock_free() << std::endl;

    uint64_t total = 4 * LoopCount;

    std::vector<std::thread> produce_threads;
    produce_threads.reserve(4);
    for (int i = 0; i < 4; ++i) {
        produce_threads.emplace_back(
                produce<Queue>,
                std::ref(task_queue), std::ref(barrier));
    }
    std::vector<std::thread> consume_threads;
    consume_threads.reserve(2);
    std::vector<std::vector<Task>> result(2);
    result[0].reserve(total / 4 * 3);
    result[1].reserve(total / 4 * 3);
    std::atomic<uint64_t> counter(0);
    for (int i = 0; i < 2; ++i) {
        consume_threads.emplace_back(
                consume<Queue>,
                std::ref(task_queue), std::ref(barrier),
                std::ref(result[i]), std::ref(counter), total);
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

    std::cout << "Result1.count = " << result[0].size() << std::endl;
    std::cout << "Result2.count = " << result[1].size() << std::endl;
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
