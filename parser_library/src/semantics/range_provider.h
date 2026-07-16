/*
 * Copyright (c) 2019 Broadcom.
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

#ifndef SEMANTICS_RANGE_PROVIDER_H
#define SEMANTICS_RANGE_PROVIDER_H

#include <span>
#include <utility>

#include "range.h"

namespace hlasm_plugin::parser_library::semantics {

// state of range adjusting
enum class adjusting_state
{
    NONE,
    SUBSTITUTION,
    MODEL_REPARSE,
};

// class for computing range
class range_provider
{
    range original_range;
    std::span<const std::pair<std::pair<size_t, bool>, range>> model_substitutions;
    std::span<const size_t> line_limits;
    adjusting_state state;

    [[nodiscard]] position adjust_model_position(position pos, bool end) const noexcept;

    [[nodiscard]] size_t get_line_limit(size_t relative_line) const noexcept;

public:
    explicit range_provider(range original_field_range, adjusting_state state);
    explicit range_provider(std::span<const std::pair<std::pair<size_t, bool>, range>> model_substitutions,
        std::span<const size_t> line_limits);
    explicit range_provider();

    [[nodiscard]] range adjust_range(range r) const noexcept;
    [[nodiscard]] const range& get_original_range() const noexcept { return original_range; }
};

template<typename It>
range text_range(const It& b, const It& e, size_t lineno_offset)
{
    assert(std::ranges::distance(b, e) >= 0);

    const auto [bx, by] = b.get_coordinates();
    position b_pos(by + lineno_offset, bx);
    if (b == e) // empty range
        return range(std::move(b_pos));
    else
    {
        const auto [ex, ey] = std::prev(e).get_coordinates();
        return range(std::move(b_pos), position(ey + lineno_offset, ex + 1));
    }
}

} // namespace hlasm_plugin::parser_library::semantics
#endif
