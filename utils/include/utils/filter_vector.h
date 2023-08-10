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
    static constexpr size_t bucket_count = bit_count - 1;
    std::array<std::vector<T>, 1 + bucket_count> filters;
    static constexpr T top_bit_shift = bit_count - 1;
    static constexpr T top_bit = (T)1 << top_bit_shift;

    static constexpr auto deconstruct_value(size_t v)
    {
        struct result_t
        {
            size_t bucket;
            size_t bit;
        };
        return result_t { 1 + (v / bit_count) % bucket_count, v % bit_count };
    }

    class global_reset_accumulator
    {
        friend class filter_vector;

        std::array<T, 1 + bucket_count> values = {};

    public:
        void reset(size_t v)
        {
            const auto [bucket, bit] = deconstruct_value(v);
            values[bucket] |= (top_bit >> bit);
        }
    };

public:
    global_reset_accumulator get_global_reset_accumulator() const { return {}; }

    static constexpr size_t effective_bit_count = bucket_count * bit_count;

    std::array<T, bucket_count> get(size_t idx) noexcept
    {
        std::array<T, bucket_count> result;
        for (size_t bucket = 1; bucket < filters.size(); ++bucket)
            result[bucket - 1] = filters[bucket][idx];
        return result;
    }
    bool get(size_t v, size_t idx) noexcept
    {
        const auto [bucket, bit] = deconstruct_value(v);
        return filters[bucket][idx] & (top_bit >> bit);
    }
    void set(const std::array<T, bucket_count>& bits, size_t idx) noexcept
    {
        T summary = 0;
        for (size_t bucket = 1; bucket < filters.size(); ++bucket)
        {
            filters[bucket][idx] = bits[bucket - 1];
            summary |= !!bits[bucket - 1] << (top_bit_shift - bucket);
        }
        summary |= !!summary << top_bit_shift;
        filters[0][idx] = summary;
    }
    void assign(size_t to, size_t from) noexcept
    {
        for (auto& f : filters)
            f[to] = f[from];
    }
    void set(size_t v, size_t idx) noexcept
    {
        const auto [bucket, bit] = deconstruct_value(v);
        filters[bucket][idx] |= (top_bit >> bit);
        filters[0][idx] |= top_bit >> bucket;
        filters[0][idx] |= top_bit;
    }
    void reset(size_t v, size_t idx) noexcept
    {
        const auto [bucket, bit] = deconstruct_value(v);
        filters[bucket][idx] &= ~(top_bit >> bit);
        const T is_empty = filters[bucket][idx] == 0;
        filters[0][idx] &= ~(is_empty << (top_bit_shift - bucket));
        filters[0][idx] &= -!!(filters[0][idx] & ~top_bit);
    }
    void reset(size_t idx) noexcept
    {
        for (auto& f : filters)
            f[idx] = 0;
    }
    void reset_global(size_t v) noexcept
    {
        const auto [bucket, bit] = deconstruct_value(v);
        const auto keep_on_mask = ~(top_bit >> bit);
        const auto summary_test_bit = top_bit >> bucket;
        const auto summary_clear_shift = top_bit_shift - bucket;

        for (auto it = filters[bucket].begin(); auto& sum : filters[0])
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
        for (size_t bucket = 1; bucket < filters.size(); ++bucket)
        {
            if (acc.values[bucket] == 0)
                continue;
            const auto keep_on_mask = ~acc.values[bucket];
            const auto summary_test_bit = top_bit >> bucket;
            const auto summary_clear_shift = top_bit_shift - bucket;

            for (auto it = filters[bucket].begin(); auto& sum : filters[0])
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
