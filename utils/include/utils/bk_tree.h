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
#include <deque>
#include <iterator>
#include <map>
#include <type_traits>
#include <vector>

namespace hlasm_plugin::utils {

template<typename T,
    class Distance,
    class Nodes = std::vector<T>,
    class Edges = std::map<std::pair<size_t, size_t>, size_t>>
class bk_tree
{
    Nodes m_nodes;
    Edges m_edges;
    [[no_unique_address]] Distance m_dist;

public:
    struct iterator
    {
        friend class bk_tree;

        const bk_tree* m_tree = nullptr;
        size_t m_idx = 0;

        iterator(const bk_tree& tree, size_t idx)
            : m_tree(&tree)
            , m_idx(idx) {};

    public:
        iterator() = default;

        auto operator<=>(const iterator& o) const noexcept
        {
            assert(m_tree == o.m_tree);
            return m_idx <=> o.m_idx;
        }
        bool operator==(const iterator& o) const noexcept
        {
            assert(m_tree == o.m_tree);
            return m_idx == o.m_idx;
        }

        iterator& operator++()
        {
            ++m_idx;
            return *this;
        }

        iterator operator++(int)
        {
            auto ret = *this;
            ++m_idx;
            return ret;
        }

        iterator& operator--()
        {
            --m_idx;
            return *this;
        }

        iterator operator--(int)
        {
            auto ret = *this;
            --m_idx;
            return ret;
        }

        iterator& operator+=(size_t off)
        {
            m_idx += off;
            return *this;
        }

        iterator& operator-=(size_t off)
        {
            m_idx -= off;
            return *this;
        }

        auto* operator->() const noexcept
        {
            assert(m_tree);
            return &m_tree->m_nodes[m_idx];
        }

        auto& operator*() const noexcept
        {
            assert(m_tree);
            return m_tree->m_nodes[m_idx];
        }

        auto& operator[](size_t n) const noexcept
        {
            assert(m_tree);
            return m_tree->m_nodes[m_idx + n];
        }

        std::ptrdiff_t operator-(const iterator& o)
        {
            assert(m_tree == o.m_tree);
            return m_idx - o.m_idx;
        }

        using iterator_category = std::iterator_traits<typename Nodes::iterator>::iterator_category;
    };

    bk_tree() requires(std::is_default_constructible_v<Distance>) = default;
    bk_tree(Distance d)
        : m_dist(std::move(d))
    {}

    auto begin() const noexcept { return iterator(*this, 0); }
    auto end() const noexcept { return iterator(*this, m_nodes.size()); }
    auto size() const noexcept { return m_nodes.size(); }

    template<typename U>
    std::pair<iterator, bool> insert(U&& value)
    {
        if (m_nodes.empty())
        {
            m_nodes.emplace_back(std::forward<U>(value));
            return { iterator(*this, 0), true };
        }

        for (size_t it = 0;;)
        {
            const auto dist = m_dist(m_nodes[it], value);
            if (dist == 0)
                return { iterator(*this, it), false };

            auto edge = m_edges.find(std::make_pair(it, dist));

            if (edge == m_edges.end())
            {
                const auto next_id = m_nodes.size();
                m_nodes.emplace_back(std::forward<U>(value));
                m_edges.try_emplace(std::make_pair(it, dist), next_id);

                return { iterator(*this, next_id), true };
            }

            it = edge->second;
        }
    }

    template<size_t extra = 0, typename U>
    auto find(U&& value) const
    {
        auto result = std::make_pair(end(), (size_t)-1);

        if (m_nodes.empty())
            return result;

        std::deque<size_t> search(1, 0);

        while (!search.empty())
        {
            const auto it = search.front();
            search.pop_front();

            const auto dist = m_dist(m_nodes[it], value);
            if (dist < result.second)
                result = { iterator(*this, it), dist };

            if (dist == 0)
                break;

            auto low = m_edges.lower_bound(std::make_pair(it, 0));
            auto high = m_edges.upper_bound(std::make_pair(it, (size_t)-1));

            for (auto e = low; e != high; ++e)
            {
                const auto abs_diff = e->first.second > dist ? e->first.second - dist : dist - e->first.second;
                if (abs_diff < result.second)
                    search.push_back(e->second);
            }
        }

        return result;
    }
};
} // namespace hlasm_plugin::utils

#endif