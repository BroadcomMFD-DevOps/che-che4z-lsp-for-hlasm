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
            const auto dist = m_dist(node->second, value);
            if (dist == 0)
                return { &node->second, false };

            key = { &node->second, dist };
        }
    }

    template<typename U>
    auto find(U&& value) const
    {
        auto result = root_key;

        if (m_nodes.empty())
            return result;

        std::vector<typename map::const_iterator> search(1, m_nodes.find(root_key));

        while (!search.empty())
        {
            const auto it = search.back();
            search.pop_back();

            const auto dist = m_dist(it->second, value);
            if (dist < result.second)
                result = { &it->second, dist };

            if (dist == 0)
                break;

            // | e->first.second - dist | < result.second
            // -result.second < e->first.second - dist < result.second
            // dist - result.second < e->first.second < dist + result.second
            // dist - result.second + 1 <= e->first.second <= dist - 1 + result.second
            //
            // truth: dist >= result.second > 0

            auto low = m_nodes.lower_bound(std::make_pair(&it->second, dist - result.second + 1));
            auto high = m_nodes.upper_bound(std::make_pair(&it->second, add(dist - 1, result.second)));

            for (auto e = low; e != high; ++e)
                search.push_back(e);
        }

        return result;
    }
};
} // namespace hlasm_plugin::utils

#endif