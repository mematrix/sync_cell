///
/// @file  copy_move_selector.hpp
/// @brief Auto select the copy or move constructor/assignment.
///

#ifndef SYNC_CELL_COPY_MOVE_SELECTOR_HPP
#define SYNC_CELL_COPY_MOVE_SELECTOR_HPP

#include <type_traits>


namespace sc::util {

namespace impl {

template<typename T, bool = std::is_copy_constructible_v<T>, bool = std::is_move_constructible_v<T>>
struct CtorSelector
{
};

template<typename T>
struct CtorSelector<T, true, true>
{
    using type = T &&;
};

template<typename T>
struct CtorSelector<T, false, true>
{
    using type = T &&;
};

template<typename T>
struct CtorSelector<T, true, false>
{
    using type = T &;
};

}

template<typename T>
[[nodiscard]] typename impl::CtorSelector<std::remove_reference_t<T>>::type
cast_ctor_ref(T &&t) noexcept
{
    return static_cast<typename impl::CtorSelector<std::remove_reference_t<T>>::type>(t);
}

}

#endif //SYNC_CELL_COPY_MOVE_SELECTOR_HPP
