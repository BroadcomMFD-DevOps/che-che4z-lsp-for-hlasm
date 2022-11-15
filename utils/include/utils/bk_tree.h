/*
 * Copyright (c) 2022 Broadcom.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *   Broadcom, Inc. - initial API and implementation
 */

#ifndef HLASMPLUGIN_UTILS_BK_TREE_H
#define HLASMPLUGIN_UTILS_BK_TREE_H

#include <algorithm>
#include <array>
#include <cassert>
#include <compare>
#include <iterator>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

namespace hlasm_plugin::utils {

template<typename T, class Distance>
class bk_tree
{
    [[no_unique_address]] Distance m_dist;

    static constexpr size_t invalid = (size_t)-1;

    struct node_t
    {
        size_t distance;
        size_t next_sibling;
        size_t first_child;
        T value;
    };

    std::vector<node_t> m_nodes;

    static constexpr auto root_key = std::pair<const T*, size_t>(nullptr, (size_t)-1);

    static constexpr auto dist = [](size_t a, size_t b) {
        if (a >= b)
            return a - b;
        else
            return b - a;
    };

    template<size_t result_size, typename U>
    void find_impl(std::array<std::pair<const T*, size_t>, result_size>& result,
        const U& value,
        size_t node_id,
        size_t distance) const noexcept
    {
        auto& best = result.front();
        while (node_id != invalid && best.second > 0)
        {
            const auto& node = m_nodes[node_id];
            node_id = node.next_sibling;

            if (dist(node.distance, distance) > best.second)
                continue;

            const auto d = m_dist(node.value, std::as_const(value));
            if (d <= best.second)
            {
                std::shift_right(result.begin(), result.end(), 1);
                best = { &node.value, d };
            }

            if (d == 0)
                break;

            if (node.first_child != invalid)
                find_impl(result, value, node.first_child, d);
        }
    }

public:
    bk_tree() requires(std::is_default_constructible_v<Distance>) = default;
    explicit bk_tree(Distance d)
        : m_dist(std::move(d))
    {}

    auto size() const noexcept { return m_nodes.size(); }
    void clear() { m_nodes.clear(); }

    template<typename U>
    std::pair<const T*, bool> insert(U&& value)
    {
        size_t dummy = m_nodes.empty() ? invalid : 0;
        size_t distance = invalid;
        auto* node_id = &dummy;
        while (*node_id != invalid)
        {
            auto& node = m_nodes[*node_id];
            if (node.distance != distance)
            {
                node_id = &node.next_sibling;
                continue;
            }
            node_id = &node.first_child;
            distance = m_dist(std::as_const(node.value), std::as_const(value));
            if (distance == 0)
                return { &node.value, false };
        }
        *node_id = m_nodes.size();
        return { &m_nodes.emplace_back(node_t { distance, invalid, invalid, std::forward<U>(value) }).value, true };
    }

    template<size_t result_size = 0, typename U>
    auto find(U&& value, size_t max_dist = (size_t)-1) const noexcept
    {
        std::array<std::pair<const T*, size_t>, result_size + !result_size> result;
        std::fill(result.begin(), result.end(), std::pair<const T*, size_t>(nullptr, max_dist));

        if (!m_nodes.empty())
            find_impl(result, std::as_const(value), 0, invalid);

        if constexpr (result_size == 0)
            return result.front();
        else
            return result;
    }
};
} // namespace hlasm_plugin::utils

#endif