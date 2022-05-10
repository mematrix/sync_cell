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
    for (uint64_t i = 0; i < LoopCount; ++i) {
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
        uint64_t count)
{
    auto tid = std::this_thread::get_id();

    sync_io([&tid] { std::cout << "[Consume] Thread [" << tid << "] waiting..." << std::endl; });
    barrier.wait(false);

    auto begin = get_current_time();

    auto c_tid = (int64_t)*(ThreadIdType *)(&tid);     // hack
    uint64_t i = 0;
    while (i < count) {
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
    sync_io([&tid, elapsed] {
        std::cout << "[Consume] Consumer Thread [" << tid << "] finished. count time: "
                  << elapsed << "ns" << std::endl;
    });
}


#endif //SYNC_CELL_QUEUE_THREAD_RUN_HPP
