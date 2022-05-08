///
/// @file  mpmc_array_queue_test.cpp
/// @brief Test for ArrayListQueue.
///

#include "queue/mpmc_array_queue.hpp"

#include "queue_thread_run.hpp"


int main()
{
    sc::mpmc::ArrayListQueue<Task> mpmc_queue;
    std::atomic_flag barrier = ATOMIC_FLAG_INIT;

    uint64_t total = 4 * LoopCount;

    std::vector<std::thread> mpmc_produce_threads;
    mpmc_produce_threads.reserve(4);
    for (int i = 0; i < 4; ++i) {
        mpmc_produce_threads.emplace_back(
                produce<sc::mpmc::ArrayListQueue<Task>>,
                std::ref(mpmc_queue), std::ref(barrier));
    }
    std::vector<std::thread> mpmc_consumer_threads;
    mpmc_consumer_threads.reserve(2);
    std::vector<std::vector<Task>> mpmc_result(2);
    mpmc_result[0].reserve(total / 4 * 3);
    mpmc_result[1].reserve(total / 4 * 3);
    std::atomic<uint64_t> mpmc_counter(0);
    for (int i = 0; i < 2; ++i) {
        mpmc_consumer_threads.emplace_back(
                consume<sc::mpmc::ArrayListQueue<Task>>,
                std::ref(mpmc_queue), std::ref(barrier),
                std::ref(mpmc_result[i]), std::ref(mpmc_counter), total);
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    // begin
    barrier.test_and_set();
    barrier.notify_all();

    for (auto &t: mpmc_produce_threads) {
        t.join();
    }
    for (auto &t: mpmc_consumer_threads) {
        t.join();
    }

    std::cout << "Result1.count = " << mpmc_result[0].size() << std::endl;
    std::cout << "Result2.count = " << mpmc_result[1].size() << std::endl;

    std::cout << "hello world" << std::endl;

    return 0;
}
