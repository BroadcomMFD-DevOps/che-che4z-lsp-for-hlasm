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

#include <cassert>

#include "antlr4-runtime.h"

#include "lexing/token.h"

using namespace hlasm_plugin::parser_library;
namespace hlasm_plugin::parser_library::semantics {

range range_provider::get_range(const antlr4::Token* start, const antlr4::Token* stop) const
{
    range ret;

    ret.start.line = start->getLine();
    ret.start.column = start->getCharPositionInLine();

    if (stop)
    {
        ret.end.line = stop->getLine();
        ret.end.column = static_cast<const lexing::token*>(stop)->get_end_of_token_in_line_utf16();
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
    const auto& [column, var] = d;
    if (var)
        return end ? r.end : r.start;

    pos.column -= column;
    pos.column += r.start.column;
    while (true)
    {
        const size_t line_limit = pos.line >= line_limits.size() ? 71 : line_limits[pos.line];
        if (pos.column < line_limit + end)
            break;
        pos.column -= line_limit - m_continued_code_line_column;
        ++pos.line;
    }
    pos.line += r.start.line;

    if (auto cmp = pos <=> r.end; cmp > 0 || end == false && cmp >= 0)
        pos = r.end;

    return pos;
}

position range_provider::adjust_position(position pos, bool end) const
{
    auto [orig_range, column] = [this, pos, end]() {
        for (auto column = pos.column - original_range.start.column; const auto& r : original_operand_ranges)
        {
            auto range_len = r.end.column - r.start.column;
            for (size_t i = r.start.line - original_range.start.line; i < r.end.line - original_range.start.line; ++i)
            {
                const size_t line_limit = i >= line_limits.size() ? 71 : line_limits[i];
                range_len += line_limit - m_continued_code_line_column;
            }
            if (column < range_len + end)
                return std::pair(r, column);
            column -= range_len;
        }
        return std::pair(original_range, pos.column - original_range.start.column);
    }();

    auto column_start = orig_range.start.column;
    size_t line_start = orig_range.start.line - original_range.start.line;

    while (true)
    {
        const size_t line_limit = line_start >= line_limits.size() ? 71 : line_limits[line_start];
        auto rest = line_limit - column_start;
        if (column < rest + end)
        {
            column_start += column;
            break;
        }
        else
        {
            column -= rest;
            column_start = m_continued_code_line_column;
            ++line_start;
        }
    }
    line_start += original_range.start.line;
    return position(line_start, column_start);
}

range_provider::range_provider(range original_range, adjusting_state state, size_t continued_code_line_column)
    : original_range(original_range)
    , state(state)
    , m_continued_code_line_column(continued_code_line_column)
{}

range_provider::range_provider(range original_field_range,
    std::vector<range> original_operand_ranges_,
    adjusting_state state,
    std::vector<size_t> line_limits,
    size_t continued_code_line_column)
    : original_range(original_field_range)
    , original_operand_ranges(std::move(original_operand_ranges_))
    , line_limits(std::move(line_limits))
    , state(state)
    , m_continued_code_line_column(continued_code_line_column)
{
    assert(original_operand_ranges.empty() || original_range.start == original_operand_ranges.front().start);
}

range_provider::range_provider(
    std::vector<std::pair<std::pair<size_t, bool>, range>> ms, std::vector<size_t> line_limits)
    : model_substitutions(std::move(ms))
    , line_limits(std::move(line_limits))
    , state(adjusting_state::MODEL_REPARSE)
{
    assert(!model_substitutions.empty());
}

range_provider::range_provider()
    : original_range()
    , state(adjusting_state::NONE)
{}
} // namespace hlasm_plugin::parser_library::semantics
