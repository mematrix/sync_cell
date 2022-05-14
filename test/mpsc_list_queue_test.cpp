///
/// @file  mpsc_list_queue_test.cpp
/// @brief Test for sc::mpsc::LinkedListQueue.
///

#include "queue/mpsc_list_queue.hpp"

#include "queue_thread_run.hpp"


constexpr uint32_t ProducerCount = 4;

int main()
{
    sc::mpsc::LinkedListQueue<Task> task_queue;
    std::atomic_flag barrier = ATOMIC_FLAG_INIT;

    std::cout << "Queue is lock free: " << std::boolalpha <<
              task_queue.is_lock_free() << std::endl;

    std::vector<std::thread> produce_threads;
    produce_threads.reserve(ProducerCount);
    for (uint32_t i = 0; i < ProducerCount; ++i) {
        produce_threads.emplace_back(
                produce<sc::mpsc::LinkedListQueue<Task>>,
                std::ref(task_queue), std::ref(barrier));
    }

    constexpr uint64_t Total = ProducerCount * LoopCount;

    std::vector<Task> result;
    result.reserve(Total);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    // begin
    barrier.test_and_set();
    barrier.notify_all();

    auto tid = std::this_thread::get_id();
    auto begin = get_current_time();

    auto c_tid = (int64_t)*(ThreadIdType *)(&tid);     // hack
    uint64_t i = 0;
    while (i < Total) {
        auto task = task_queue.try_dequeue();
        if (task) {
            task->consume_tid = c_tid;
            task->out_time = get_current_time();
            result.push_back(*task);

            ++i;
        }
    }

    auto end = get_current_time();
    auto elapsed = end - begin;

    for (auto &t: produce_threads) {
        t.join();
    }

    std::cout << "[Consume] Consumer Thread [" << tid << "] finished. count time: "
              << elapsed << "ns" << std::endl;

    std::cout << "hello world" << std::endl;

    return 0;
}
