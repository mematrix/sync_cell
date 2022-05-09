///
/// @file  mpmc_array_queue_test.cpp
/// @brief Test for ArrayListQueue.
///

#include "queue/mpmc_array_queue.hpp"

#include "queue_thread_run.hpp"


constexpr uint32_t ProducerCount = 4;
constexpr uint32_t ConsumerCount = 2;

int main()
{
    sc::mpmc::ArrayListQueue<Task> mpmc_queue;
    std::atomic_flag barrier = ATOMIC_FLAG_INIT;

    uint64_t total = ProducerCount * LoopCount;

    std::vector<std::thread> mpmc_produce_threads;
    mpmc_produce_threads.reserve(ProducerCount);
    for (uint32_t i = 0; i < ProducerCount; ++i) {
        mpmc_produce_threads.emplace_back(
                produce<sc::mpmc::ArrayListQueue<Task>>,
                std::ref(mpmc_queue), std::ref(barrier));
    }
    std::vector<std::thread> mpmc_consumer_threads;
    mpmc_consumer_threads.reserve(ConsumerCount);
    std::vector<std::vector<Task>> mpmc_result(ConsumerCount);
    for (auto &r : mpmc_result) {
        r.reserve(total / ConsumerCount / 2 * 3);
    }
    std::atomic<uint64_t> mpmc_counter(0);
    for (uint32_t i = 0; i < ConsumerCount; ++i) {
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

    for (size_t i = 0; i < mpmc_result.size(); ++i) {
        std::cout << "Result" << i << ".count = " << mpmc_result[i].size() << std::endl;
    }

    std::cout << "hello world" << std::endl;

    return 0;
}
