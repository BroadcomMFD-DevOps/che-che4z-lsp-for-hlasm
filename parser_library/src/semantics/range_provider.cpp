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

#include "range_provider.h"

#include <algorithm>
#include <cassert>

using namespace hlasm_plugin::parser_library;
namespace hlasm_plugin::parser_library::semantics {

range range_provider::adjust_range(range r) const noexcept
{
    using enum adjusting_state;
    switch (state)
    {
        default:
            assert(false);
            [[fallthrough]];

        case adjusting_state::NONE:
            return r;

        case adjusting_state::SUBSTITUTION:
            return original_range;

        case adjusting_state::MODEL_REPARSE:
            assert(r.start.line == 0 && r.end.line == 0);
            if (r.start != r.end)
                return range(adjust_model_position(r.start, false), adjust_model_position(r.end, true));

            auto adjusted = adjust_model_position(r.end, true);
            return range(adjusted, adjusted);
    }
}

position range_provider::adjust_model_position(position pos, bool end) const noexcept
{
    static constexpr size_t continued_code_line_column = 15;

    const auto& [d, r] = *std::prev(std::find_if(std::next(model_substitutions.begin()),
        model_substitutions.end(),
        [pos, end](const auto& s) { return pos.column < s.first.first + end; }));
    const auto& [column, var] = d;
    if (var)
        return end ? r.end : r.start;

    pos.column -= column;
    pos.column += r.start.column;
    // TODO: This should be handled by per-line line mapping
    pos.line = r.start.line - model_substitutions.front().second.start.line;
    while (pos.line < line_limits.size())
    {
        const size_t line_limit = line_limits[pos.line];
        if (pos.column < line_limit + end)
            break;
        pos.column -= line_limit - continued_code_line_column;
        ++pos.line;
    }
    pos.line += r.start.line;

    if (auto cmp = pos <=> r.end; cmp > 0 || (end == false && cmp >= 0))
        pos = r.end;

    return pos;
}

range_provider::range_provider(range original_range, adjusting_state state)
    : original_range(original_range)
    , state(state)
{}

range_provider::range_provider(
    std::span<const std::pair<std::pair<size_t, bool>, range>> ms, std::span<const size_t> line_limits)
    : model_substitutions(ms)
    , line_limits(line_limits)
    , state(adjusting_state::MODEL_REPARSE)
{
    assert(!model_substitutions.empty());
}

range_provider::range_provider()
    : original_range()
    , state(adjusting_state::NONE)
{}
} // namespace hlasm_plugin::parser_library::semantics
