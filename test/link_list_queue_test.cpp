///
/// @file  link_list_queue_test.cpp
/// @brief Test of 'LinkListQueue'.
///

#include "queue/link_list_queue.hpp"
#include "queue/link_list_queue_v2.hpp"
#include "queue/block_list_queue.hpp"

#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>


constexpr int64_t LoopCount = 10'000'000;

struct Task
{
    int64_t tid;
    int64_t consume_tid;
    int64_t task_id;
    int64_t in_time;
    int64_t out_time;
};

static int64_t get_current_time()
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
}

static std::mutex IOMtx;

template<typename Func>
void sync_io(Func f)
{
    std::lock_guard guard(IOMtx);
    f();
}

class QueueTest
{
public:
    QueueTest()
    {
        head_ = new Node;
        tail_ = head_;
    }

    ~QueueTest()
    {
        auto *h = head_;
        while (h) {
            auto tmp = h;
            h = h->next;
            delete tmp;
        }
    }

    void enqueue(const Task &t)
    {
        auto *node = new Node(t);
        tail_->next = node;
        tail_ = node;
    }

private:
    struct Node
    {
        Node *next = nullptr;
        std::optional<Task> task{};

        Node() = default;

        explicit Node(const Task &t) : task(std::in_place, t) { }
    };

    Node *head_;
    Node *tail_;
};

static void produce_single(std::atomic_flag &barrier)
{
    auto tid = std::this_thread::get_id();
    QueueTest qt;

    sync_io([&tid] { std::cout << "[Single] Thread [" << tid << "] waiting..." << std::endl; });
    barrier.wait(false);
    std::cout << tid << std::endl;

    auto begin = get_current_time();

    Task task{};
    task.tid = (int64_t)*(std::thread::native_handle_type *)(&tid);     // hack
    task.out_time = 0;
    task.consume_tid = 0;
    for (int64_t i = 0; i < LoopCount; ++i) {
        task.task_id = i;
        task.in_time = get_current_time();
//        task_queue.enqueue(task);
        qt.enqueue(task);
    }

    auto end = get_current_time();
    auto elapsed = end - begin;
    sync_io([&tid, elapsed] {
        std::cout << "[Single] Thread [" << tid << "] finished. total time: " <<
                  elapsed << "ns" << std::endl;
    });
}

template<typename Queue, const char *TAG>
static void produce(Queue &task_queue, std::atomic_flag &barrier)
{
    auto tid = std::this_thread::get_id();

    sync_io([&tid] { std::cout << '[' << TAG << "] Thread [" << tid << "] waiting..." << std::endl; });
    barrier.wait(false);
    std::cout << tid << std::endl;

    auto begin = get_current_time();

    Task task{};
    task.tid = (int64_t)*(std::thread::native_handle_type *)(&tid);     // hack
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
        std::cout << '[' << TAG << "] Thread [" << tid << "] finished. total time: " <<
                  elapsed << "ns" << std::endl;
    });
}

template<typename Queue, const char *TAG>
static void consume(
        Queue &task_queue,
        std::atomic_flag &barrier,
        std::vector<Task> &result,
        std::atomic<uint64_t> &counter,
        uint64_t total)
{
    auto tid = std::this_thread::get_id();

    sync_io([&tid] { std::cout << '[' << TAG << "] Thread [" << tid << "] waiting..." << std::endl; });
    barrier.wait(false);
    std::cout << tid << std::endl;

    auto begin = get_current_time();

    auto c_tid = (int64_t)*(std::thread::native_handle_type *)(&tid);     // hack
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
        std::cout << '[' << TAG << "] Consumer Thread [" << tid << "] finished. total time: "
                  << elapsed << "ns" << std::endl;
    });
}

static const char LinkListTag[] = "L/MPSC";
static const char BlockListTag[] = "B/MPMC";

int main(int argc, char **argv)
{
    sc::LinkListQueue<Task> task_queue;
    std::atomic_flag barrier = ATOMIC_FLAG_INIT;

    std::cout << "Queue is lock free: " << std::boolalpha <<
              task_queue.is_lock_free() << std::endl;

    uint64_t total = 4 * LoopCount;

    std::thread single_queue(produce_single, std::ref(barrier));

    std::vector<std::thread> produce_threads;
    produce_threads.reserve(4);
    for (int i = 0; i < 4; ++i) {
        produce_threads.emplace_back(
                produce<sc::LinkListQueue<Task>, LinkListTag>,
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
                consume<sc::LinkListQueue<Task>, LinkListTag>,
                std::ref(task_queue), std::ref(barrier),
                std::ref(result[i]), std::ref(counter), total);
    }

    sc::mpmc::BlockListQueue<Task> mpmc_queue;

    std::vector<std::thread> mpmc_produce_threads;
    mpmc_produce_threads.reserve(4);
    for (int i = 0; i < 4; ++i) {
        mpmc_produce_threads.emplace_back(
                produce<sc::mpmc::BlockListQueue<Task>, BlockListTag>,
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
                consume<sc::mpmc::BlockListQueue<Task>, BlockListTag>,
                std::ref(mpmc_queue), std::ref(barrier),
                std::ref(mpmc_result[i]), std::ref(mpmc_counter), total);
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    // begin
    barrier.test_and_set();
    barrier.notify_all();

    single_queue.join();
    for (auto &t: produce_threads) {
        t.join();
    }
    for (auto &t: consume_threads) {
        t.join();
    }
    for (auto &t: mpmc_produce_threads) {
        t.join();
    }
    for (auto &t: mpmc_consumer_threads) {
        t.join();
    }

    std::cout << "Result1.count = " << result[0].size() << std::endl;
    std::cout << "Result2.count = " << result[1].size() << std::endl;
    std::cout << "[MPMC] Result1.count = " << mpmc_result[0].size() << std::endl;
    std::cout << "[MPMC] Result2.count = " << mpmc_result[1].size() << std::endl;

    std::cout << "hello world" << std::endl;

    return 0;
}
