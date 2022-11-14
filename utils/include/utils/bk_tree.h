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
    using map = std::map<std::pair<const T*, size_t>, T>;
    map m_nodes;
    [[no_unique_address]] Distance m_dist;

    static constexpr auto root_key = std::pair<const T*, size_t>(nullptr, (size_t)-1);

    static constexpr auto add = [](size_t a, size_t b) {
        size_t res = a + b;
        if (res < a)
            return (size_t)-1;
        return res;
    };

public:
    bk_tree() requires(std::is_default_constructible_v<Distance>) = default;
    bk_tree(Distance d)
        : m_dist(std::move(d))
    {}

    auto size() const noexcept { return m_nodes.size(); }
    void clear() { m_nodes.clear(); }

    template<typename U>
    std::pair<const T*, bool> insert(U&& value)
    {
        auto key = root_key;

        while (true)
        {
            auto node = m_nodes.find(key);
            if (node == m_nodes.end())
            {
                auto [it, _] = m_nodes.try_emplace(key, std::forward<U>(value));
                return { &it->second, true };
            }
            const auto dist = m_dist(std::as_const(node->second), std::as_const(value));
            if (dist == 0)
                return { &node->second, false };

            key = { &node->second, dist };
        }
    }

    template<size_t result_size = 0, typename U>
    auto find(U&& value, size_t max_dist = (size_t)-1) const
    {
        std::array<std::pair<const T*, size_t>, result_size + !result_size> result;
        std::fill(result.begin(), result.end(), std::pair<const T*, size_t>(nullptr, max_dist));
        auto& best = result.front();

        std::vector<std::pair<typename map::const_iterator, typename map::const_iterator>> search;
        if (auto root = m_nodes.find(root_key); root != m_nodes.end())
            search.emplace_back(root, std::next(root));

        while (!search.empty())
        {
            auto& it_pair = search.back();
            const auto it = it_pair.first++;
            if (it_pair.first == it_pair.second)
                search.pop_back();

            const auto dist = m_dist(it->second, std::as_const(value));
            if (dist <= best.second)
            {
                std::shift_right(result.begin(), result.end(), 1);
                best = { &it->second, dist };
            }

            if (dist == 0)
                break;

            auto low = m_nodes.lower_bound(std::make_pair(&it->second, dist - best.second));
            auto high = m_nodes.upper_bound(std::make_pair(&it->second, add(dist, best.second)));

            if (low != high)
                search.emplace_back(low, high);
        }

        if constexpr (result_size == 0)
            return best;
        else
            return result;
    }
};
} // namespace hlasm_plugin::utils

#endif