///
/// @file  queue_thread_run.hpp
/// @brief Produce thread work and consume thread work.
///

#ifndef SYNC_CELL_QUEUE_THREAD_RUN_HPP
#define SYNC_CELL_QUEUE_THREAD_RUN_HPP

#include <thread>
#include <vector>

#include "test_util.hpp"


#if defined(_MSC_VER)
using ThreadIdType = unsigned int;
#else
using ThreadIdType = std::thread::native_handle_type;
#endif

template<typename Queue>
void produce(Queue &task_queue, std::atomic_flag &barrier)
{
    auto tid = std::this_thread::get_id();

    sync_io([&tid] { std::cout << "[Produce] Thread [" << tid << "] waiting..." << std::endl; });
    barrier.wait(false);

    auto begin = get_current_time();

    Task task{};
    task.tid = (int64_t)*(ThreadIdType *)(&tid);     // hack
    task.out_time = 0;
    task.consume_tid = 0;
    for (int64_t i = 0; i < LoopCount; ++i) {
        task.task_id = i;
        task.in_time = get_current_time();
        task_queue.enqueue(task);
    }

    auto end = get_current_time();
    auto elapsed = end - begin;
    sync_io([&tid, elapsed] {
        std::cout << "[Produce] Thread [" << tid << "] finished. total time: " <<
                  elapsed << "ns" << std::endl;
    });
}

template<typename Queue>
void consume(
        Queue &task_queue,
        std::atomic_flag &barrier,
        std::vector<Task> &result,
        std::atomic<uint64_t> &counter,
        uint64_t total)
{
    auto tid = std::this_thread::get_id();

    sync_io([&tid] { std::cout << "[Consume] Thread [" << tid << "] waiting..." << std::endl; });
    barrier.wait(false);

    auto begin = get_current_time();

    auto c_tid = (int64_t)*(ThreadIdType *)(&tid);     // hack
    while (counter.load(std::memory_order_acquire) != total) {
        auto task = task_queue.try_dequeue();
        if (task) {
            counter.fetch_add(1, std::memory_order_acq_rel);
            task->consume_tid = c_tid;
            task->out_time = get_current_time();
            result.push_back(*task);
        }
    }

    auto end = get_current_time();
    auto elapsed = end - begin;
    sync_io([&tid, elapsed] {
        std::cout << "[Consume] Consumer Thread [" << tid << "] finished. total time: "
                  << elapsed << "ns" << std::endl;
    });
}


#endif //SYNC_CELL_QUEUE_THREAD_RUN_HPP
