///
/// @file  cache_padded.hpp
/// @brief Pads and aligns a value to the length of a cache line. Inspired by crossbeam-rs:
/// [https://github.com/crossbeam-rs/crossbeam/blob/master/crossbeam-utils/src/cache_padded.rs](CachePadded)
///

#ifndef SYNC_CELL_CACHE_PADDED_HPP
#define SYNC_CELL_CACHE_PADDED_HPP

#include <new>  // std::hardware_destructive_interference_size
#include <type_traits>
#include <utility>


namespace sc::util {

namespace impl {

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_constructive_interference_size;
using std::hardware_destructive_interference_size;
#else
// 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
constexpr std::size_t hardware_constructive_interference_size = 64;
constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

}

template<typename T>
class alignas(impl::hardware_destructive_interference_size) CachePadded
{
    static_assert(!std::is_reference_v<T>);

    template<typename U>
    using not_self = std::negation<std::is_same<CachePadded, std::remove_cvref_t<U>>>;

public:
    using value_type = T;

    template<std::enable_if_t<std::is_default_constructible_v<T>, bool> = false>
    constexpr CachePadded() noexcept(std::is_nothrow_default_constructible_v<T>) : value_() { };

    template<typename... Args, std::enable_if_t<std::is_constructible_v<T, Args...>, bool> = false>
    explicit constexpr CachePadded(std::in_place_t, Args... args)
    noexcept(std::is_nothrow_constructible_v<T, Args...>)
            : value_(std::forward<Args>(args)...) { }

    template<typename U = T,
            std::enable_if_t<std::conjunction_v<not_self<U>, std::is_constructible<T, U>>, bool> = false>
    explicit constexpr CachePadded(U &&u)
    noexcept(std::is_nothrow_constructible_v<T, U>)
            : value_(std::forward<U>(u)) { }

    template<typename U = T, std::enable_if_t<std::is_constructible_v<T, const U &>, bool> = false>
    explicit constexpr CachePadded(const CachePadded<U> &other)
    noexcept(std::is_nothrow_constructible_v<T, const U &>)
            : value_(other.value_) { }

    template<typename U = T, std::enable_if_t<std::is_constructible_v<T, U>, bool> = false>
    explicit constexpr CachePadded(CachePadded<U> &&other)
    noexcept(std::is_nothrow_constructible_v<T, U>)
            : value_(std::move(other.value_)) { }

    template<typename U = T, std::enable_if_t<std::conjunction_v<not_self<U>, std::is_assignable<T &, U>>, bool> = true>
    CachePadded &operator=(U &&u)
    noexcept(std::is_nothrow_assignable_v<T &, U>)
    {
        value_ = std::forward<U>(u);
        return *this;
    }

    template<typename U = T, std::enable_if_t<std::is_assignable_v<T &, const U &>, bool> = false>
    CachePadded &operator=(const CachePadded<U> &other)
    {
        value_ = other.value_;
        return *this;
    }

    template<typename U = T, std::enable_if_t<std::is_assignable_v<T &, U>, bool> = false>
    CachePadded &operator=(CachePadded<U> &&other)
    {
        value_ = std::move(other.value_);
        return *this;
    }

    constexpr const T *operator->() const noexcept
    {
        return std::addressof(value_);
    }

    constexpr T *operator->() noexcept
    {
        return std::addressof(value_);
    }

    constexpr const T &operator*() const & noexcept
    {
        return value_;
    }

    constexpr T &operator*() & noexcept
    {
        return value_;
    }

    constexpr const T &&operator*() const && noexcept
    {
        return std::move(value_);
    }

    constexpr T &&operator*() && noexcept
    {
        return std::move(value_);
    }

    constexpr const T &value() const & noexcept
    {
        return value_;
    }

    constexpr T &value() & noexcept
    {
        return value_;
    }

    constexpr const T &&value() const && noexcept
    {
        return value_;
    }

    constexpr T &&value() && noexcept
    {
        return value_;
    }

private:
    value_type value_;
};

}

#endif //SYNC_CELL_CACHE_PADDED_HPP
