/*
 * Copyright (c) 2023 Broadcom.
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

#ifndef HLASMPLUGIN_UTILS_FILTER_VECTOR_H
#define HLASMPLUGIN_UTILS_FILTER_VECTOR_H

#include <array>
#include <concepts>
#include <cstddef>
#include <limits>
#include <vector>

namespace hlasm_plugin::utils {

template<std::unsigned_integral T>
class filter_vector
{
    static constexpr size_t bit_count = std::numeric_limits<T>::digits;
    std::array<std::vector<T>, bit_count> filters;
    static constexpr T top_bit_shift = bit_count - 1;
    static constexpr T top_bit = (T)1 << top_bit_shift;

    struct global_reset_accumulator
    {
        std::array<T, bit_count> values = {};

        void reset(size_t bit) { values[1 + bit / bit_count] |= (top_bit >> bit % bit_count); }
    };

public:
    global_reset_accumulator get_global_reset_accumulator() const { return {}; }

    static constexpr size_t effective_bit_count = top_bit_shift * bit_count;

    std::array<T, bit_count - 1> get(size_t idx) noexcept
    {
        std::array<T, bit_count - 1> result;
        for (size_t i = 1; i < bit_count; ++i)
            result[i - 1] = filters[i][idx];
        return result;
    }
    bool get(size_t bit, size_t idx) noexcept
    {
        return filters[1 + bit / bit_count][idx] & (top_bit >> bit % bit_count);
    }
    void set(const std::array<T, bit_count - 1>& bits, size_t idx) noexcept
    {
        T summary = 0;
        for (size_t i = 0; i < bit_count - 1; ++i)
        {
            filters[i + 1][idx] = bits[i];
            summary |= !!bits[i] << (top_bit_shift - 1 - i);
        }
        summary |= !!summary << top_bit_shift;
    }
    void assign(size_t to, size_t from) noexcept
    {
        for (auto& f : filters)
            f[to] = f[from];
    }
    void set(size_t bit, size_t idx) noexcept
    {
        filters[1 + bit / bit_count][idx] |= (top_bit >> bit % bit_count);
        filters[0][idx] |= top_bit >> (1 + bit / bit_count);
        filters[0][idx] |= top_bit;
    }
    void reset(size_t bit, size_t idx) noexcept
    {
        auto& vec = filters[1 + bit / bit_count];

        vec[idx] &= ~(top_bit >> bit % bit_count);
        const T is_empty = vec[idx] == 0;
        auto& v = filters[0][idx];
        v &= ~(is_empty << (top_bit_shift - 1 - bit / bit_count));
        v &= -!!(filters[0][idx] & ~top_bit);
    }
    void reset(size_t idx) noexcept
    {
        for (auto& f : filters)
            f[idx] = 0;
    }
    void reset_global(size_t bit) noexcept
    {
        const auto keep_on_mask = ~(top_bit >> bit % bit_count);
        const auto summary_test_bit = top_bit >> (1 + bit / bit_count);
        const auto summary_clear_shift = (top_bit_shift - 1 - bit / bit_count);

        for (auto it = filters[1 + bit / bit_count].begin(); auto& sum : filters[0])
        {
            auto& b = *it++;
            if (!(sum & summary_test_bit))
                continue;
            b &= keep_on_mask;
            const T is_empty = b == 0;
            sum &= ~(is_empty << summary_clear_shift);
            sum &= static_cast<T>(-!!(sum & ~top_bit));
        }
    }

    void reset_global(const global_reset_accumulator& acc) noexcept
    {
        const auto& v = acc.values;
        for (size_t i = 1; i < bit_count; ++i)
        {
            if (v[i] == 0)
                continue;
            const auto keep_on_mask = ~v[i];
            const auto summary_test_bit = top_bit >> i;
            const auto summary_clear_shift = (top_bit_shift - i);

            for (auto it = filters[i].begin(); auto& sum : filters[0])
            {
                auto& b = *it++;
                if (!(sum & summary_test_bit))
                    continue;
                b &= keep_on_mask;
                const T is_empty = b == 0;
                sum &= ~(is_empty << summary_clear_shift);
                sum &= static_cast<T>(-!!(sum & ~top_bit));
            }
        }
    }

    bool any(size_t idx) const noexcept { return filters[0][idx] & top_bit; }

    void emplace_back()
    {
        for (auto& f : filters)
            f.emplace_back(0);
    }

    void pop_back() noexcept
    {
        for (auto& f : filters)
            f.pop_back();
    }

    void swap(size_t l, size_t r) noexcept
    {
        if (((filters[0][l] | filters[0][r]) & top_bit) == 0)
            return;
        for (auto& f : filters)
            std::swap(f[l], f[r]);
    }

    auto size() const noexcept { return filters.front().size(); }

    void clear() noexcept
    {
        for (auto& f : filters)
            f.clear();
    }
};

} // namespace hlasm_plugin::utils

#endif
