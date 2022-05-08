///
/// @file  single_produce_test.cpp
/// @brief Run the single-threaded test. No additional synchronization mechanism.
///

#include "test_util.hpp"

#include <optional>
#include <thread>


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

static void produce_single()
{
    auto tid = std::this_thread::get_id();
    QueueTest qt;

    sync_io([&tid] { std::cout << "[Single] Thread [" << tid << "] waiting..." << std::endl; });
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto begin = get_current_time();

    Task task{};
    task.tid = 0;
    task.out_time = 0;
    task.consume_tid = 0;
    for (int64_t i = 0; i < LoopCount; ++i) {
        task.task_id = i;
        task.in_time = get_current_time();
        qt.enqueue(task);
    }

    auto end = get_current_time();
    auto elapsed = end - begin;
    sync_io([&tid, elapsed] {
        std::cout << "[Single] Thread [" << tid << "] finished. total time: " <<
                  elapsed << "ns" << std::endl;
    });
}

int main()
{
    produce_single();

    std::cout << "hello world" << std::endl;

    return 0;
}
