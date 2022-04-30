///
/// @file  link_list_node.hpp
/// @brief
///

#ifndef SYNC_CELL_LINK_LIST_NODE_HPP
#define SYNC_CELL_LINK_LIST_NODE_HPP

#include <atomic>
#include <optional>


namespace sc::impl {

template<typename T>
struct Node
{
    std::atomic<Node *> next{nullptr};
    std::optional<T> value;

    constexpr Node() = default;

    template<typename... Args>
    explicit Node(std::in_place_t, Args &&... args) : value(std::in_place, std::forward<Args>(args)...)
    {
        static_assert(std::is_constructible_v<T, Args &&...>);
    }
};

}

#endif //SYNC_CELL_LINK_LIST_NODE_HPP
