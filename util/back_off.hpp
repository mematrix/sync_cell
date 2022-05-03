///
/// @file  back_off.hpp
/// @brief Port from the crossbeam-rs project:
/// [https://github.com/crossbeam-rs/crossbeam/blob/master/crossbeam-utils/src/backoff.rs](Backoff).
///

#ifndef SYNC_CELL_BACK_OFF_HPP
#define SYNC_CELL_BACK_OFF_HPP

#include <algorithm>
#include <cstdint>
#include <thread>

#include "intrin_wrapper.hpp"


namespace sc::util {

/// @brief Performs exponential backoff in spin loops.
///
/// Backing off in spin loops reduces contention and improves overall performance.
///
/// This primitive can execute *YIELD* and *PAUSE* instructions, yield the current thread to the OS
/// scheduler, and tell when is a good time to block the thread using a different synchronization
/// mechanism. Each step of the back off procedure takes roughly twice as long as the previous
/// step.
///
/// @example
/// Backing off in a lock-free loop: \n
/// ``` cpp
/// size_t fetch_mul(const std::atomic<size_t> &a, size_t b) {
///     Backoff backoff;
///     auto val = a.load();
///     while (true) {
///         if (a.compare_exchange_strong(val, val * b)) {
///             return val;
///         }
///         backoff.spin();
///     }
/// }
/// ```
///
/// @example
/// Waiting for an @c std::atomic_bool to become 'true': \n
/// ``` cpp
/// void spin_wait(const std::atomic<bool> &ready) {
///     Backoff backoff;
///     while (!ready.load()) { backoff.snooze(); }
/// }
/// ```
///
/// @example
/// Waiting for an @c std::atomic_bool to become 'true' and parking the thread after a long wait.
/// Note that whoever sets the atomic variable to 'true' must notify the parked thread by calling
/// @c notify_all or @c notify_one method of @c std::atomic. \n
/// ``` cpp
/// void blocking_wait(const std::atomic<bool> &ready) {
///     Backoff backoff;
///     while (!ready.load()) {
///         if (backoff.is_completed()) { ready.wait(false); }
///         else { backoff.snooze(); }
///     }
/// }
/// ```
class Backoff
{
    static constexpr uint32_t SpinLimit = 6;
    static constexpr uint32_t YieldLimit = 10;

public:
    constexpr Backoff() noexcept = default;

    void reset() noexcept
    {
        step_ = 0;
    }

    /// @brief Backs off in a lock-free loop.
    ///
    /// This method should be used when we need to retry an operation because another thread made
    /// progress.
    void spin() noexcept
    {
        auto s = std::min(step_, SpinLimit);
        s = 1 << s;
        for (uint32_t i = 0; i < s; ++i) {
            spin_loop_hint();
        }

        if (step_ <= SpinLimit) {
            ++step_;
        }
    }

    /// @brief Backs off in a blocking loop.
    ///
    /// This method should be used when we need to wait for another thread to make progress.
    ///
    /// The processor may yield using the *YIELD* or *PAUSE* instruction and the current thread
    /// may yield by giving up a time-slice to the OS scheduler.
    ///
    /// @note If possible, use @c is_completed to check when it is advised to stop using backoff
    /// and block the current thread using a different synchronization mechanism instead.
    void snooze() noexcept
    {
        if (step_ <= SpinLimit) {
            auto s = 1u << step_;
            for (uint32_t i = 0; i < s; ++i) {
                spin_loop_hint();
            }
        } else {
            std::this_thread::yield();
        }

        if (step_ <= YieldLimit) {
            ++step_;
        }
    }

    /// @brief Returns true if exponential backoff has completed and blocking the thread is advised.
    [[nodiscard]] bool is_completed() const noexcept
    {
        return step_ > YieldLimit;
    }

private:
    uint32_t step_ = 0;
};

}

#endif //SYNC_CELL_BACK_OFF_HPP
