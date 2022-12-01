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

using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::parser_library::semantics;

range range_provider::get_range(const antlr4::Token* start, const antlr4::Token* stop) const
{
    range ret;

    ret.start.line = start->getLine();
    ret.start.column = start->getCharPositionInLine();

    if (stop)
    {
        ret.end.line = stop->getLine();
        ret.end.column = stop->getCharPositionInLine() + stop->getStopIndex() - stop->getStartIndex() + 1;
    }
    else // empty rule
    {
        ret.end = ret.start;
    }
    return adjust_range(ret);
}

range range_provider::get_range(const antlr4::Token* terminal) const { return get_range(terminal, terminal); }

range range_provider::get_range(antlr4::ParserRuleContext* non_terminal) const
{
    return get_range(non_terminal->getStart(), non_terminal->getStop());
}

range range_provider::get_empty_range(const antlr4::Token* start) const
{
    range ret;
    ret.start.line = start->getLine();
    ret.start.column = start->getCharPositionInLine();
    ret.end = ret.start;
    return adjust_range(ret);
}

range range_provider::adjust_range(range r) const
{
    if (state == adjusting_state::MACRO_REPARSE)
    {
        if (r.start != r.end)
            return range(adjust_position(r.start, false), adjust_position(r.end, true));

        auto adjusted = adjust_position(r.end, true);
        return range(adjusted, adjusted);
    }
    else if (state == adjusting_state::SUBSTITUTION)
        return original_range;
    else if (state == adjusting_state::NONE)
        return r;
    else if (state == adjusting_state::MODEL_REPARSE)
    {
        assert(r.start.line == 0 && r.end.line == 0);
        if (r.start != r.end)
            return range(adjust_model_position(r.start, false), adjust_model_position(r.end, true));

        auto adjusted = adjust_model_position(r.end, true);
        return range(adjusted, adjusted);
    }
    assert(false);
    return r;
}

position range_provider::adjust_model_position(position pos, bool end) const
{
    const auto& [d, r] = *std::prev(std::find_if(std::next(model_substitutions.begin()),
        model_substitutions.end(),
        [pos, end](const auto& s) { return pos.column < s.first.first + end; }));
    const auto& [offset, var] = d;
    if (var)
        return end ? r.end : r.start;

    pos.column -= offset;
    pos.column += r.start.column;
    pos.line += r.start.line;
    while (pos.column >= 71 + end)
    {
        pos.column -= 71 - 15;
        pos.line += 1;
    }

    if (auto cmp = pos <=> r.end; cmp > 0 || end == false && cmp >= 0)
        pos = r.end;

    return pos;
}

position range_provider::adjust_position(position pos, bool end) const
{
    auto [orig_range, offset] = [this, pos, end]() {
        const size_t continued_code_line_width = 72 - continued_code_line_offset - 1;

        for (auto offset = pos.column - original_range.start.column; const auto& r : original_operand_ranges)
        {
            auto range_len = r.end.column - r.start.column + continued_code_line_width * (r.end.line - r.start.line);
            if (offset < range_len + end)
                return std::pair(r, offset);
            offset -= range_len;
        }
        return std::pair(original_range, pos.column - original_range.start.column);
    }();

    auto column_start = orig_range.start.column;
    auto line_start = orig_range.start.line;

    while (true)
    {
        auto rest = 71 - column_start;
        if (offset < rest + end)
        {
            column_start += offset;
            break;
        }
        else
        {
            offset -= rest;
            column_start = continued_code_line_offset;
            ++line_start;
        }
    }
    return position(line_start, column_start);
}

range_provider::range_provider(range original_range, adjusting_state state, const size_t continued_code_line_offset)
    : original_range(original_range)
    , state(state)
    , continued_code_line_offset(continued_code_line_offset)
{}

range_provider::range_provider(range original_field_range,
    std::vector<range> original_operand_ranges_,
    adjusting_state state,
    const size_t continued_code_line_offset)
    : original_range(original_field_range)
    , original_operand_ranges(std::move(original_operand_ranges_))
    , state(state)
    , continued_code_line_offset(continued_code_line_offset)
{
    assert(original_operand_ranges.empty() || original_range.start == original_operand_ranges.front().start);
}

hlasm_plugin::parser_library::semantics::range_provider::range_provider(
    std::vector<std::pair<std::pair<size_t, bool>, range>> ms)
    : model_substitutions(std::move(ms))
    , state(adjusting_state::MODEL_REPARSE)
{
    assert(!model_substitutions.empty());
}

range_provider::range_provider()
    : original_range()
    , state(adjusting_state::NONE)
{}
