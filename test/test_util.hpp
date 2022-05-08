///
/// @file  test_util.hpp
/// @brief Common util functions for test.
///

#ifndef SYNC_CELL_TEST_UTIL_HPP
#define SYNC_CELL_TEST_UTIL_HPP

#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>


constexpr int64_t LoopCount = 10'000'000;

struct Task
{
    int64_t tid;
    int64_t consume_tid;
    int64_t task_id;
    int64_t in_time;
    int64_t out_time;
};

inline int64_t get_current_time()
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
}

inline std::mutex IOMtx;

template<typename Func>
void sync_io(Func f)
{
    std::lock_guard guard(IOMtx);
    f();
}

#endif //SYNC_CELL_TEST_UTIL_HPP
