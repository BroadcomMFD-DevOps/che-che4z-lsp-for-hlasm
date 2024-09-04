/*
 * Copyright (c) 2024 Broadcom.
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

#include "new_parser.h"

#include <algorithm>
#include <cassert>

#include "lexing/logical_line.h"
#include "parser_impl.h"
#include "semantics/range_provider.h"
#include "utils/unicode_text.h"

namespace hlasm_plugin::parser_library::parsing {

void new_parser::clear()
{
    m_comments.clear();
    m_text_ranges.clear();
    m_text.clear();
}

using counter = utils::utf8_multicounter<utils::utf8_utf16_counter, utils::utf8_utf32_counter>;
static constexpr auto utf16 = utils::counter_index<0>;
static constexpr auto utf32 = utils::counter_index<1>;

namespace {
void skip_spaces(utils::utf8_iterator<std::string_view::const_iterator, counter>& it,
    const std::string_view::const_iterator e,
    size_t& lineno)
{
    while (it != e)
    {
        const auto c = *it;

        if (c == '\n')
        {
            ++lineno;
            it = utils::utf8_iterator(std::next(it.base()), counter());
            utils::utf8_next(it, lexing::default_ictl.continuation - 1, e);
            continue;
        }
        else if (c == '\r')
        {
            ++lineno;
            if (++it != e && *it == '\n')
                ++it;
            it = utils::utf8_iterator(it.base(), counter());
            utils::utf8_next(it, lexing::default_ictl.continuation - 1, e);
            continue;
        }
        else if (it.counter(utf32) < lexing::default_ictl.end && c != ' ')
            break;

        ++it;
    }
}
} // namespace

std::span<range> new_parser::noop_operands(
    std::string_view text, position p, size_t logical_column, semantics::range_provider& rp)
{
    assert(logical_column > 0 && p.column > 0);

    if (text.empty())
        return {};

    clear();

    auto lineno = p.line;

    utils::utf8_iterator it(text.cbegin(), counter(p.column, logical_column));
    const auto e = text.cend();

    skip_spaces(it, e, lineno);

    if (it == e)
        return {};

    const position start(lineno, it.counter(utf16));
    position end = start;

    while (it != e)
    {
        const bool code = it.counter(utf32) < lexing::default_ictl.end;
        const auto c = *it;
        ++it;
        if (code)
            end.column = it.counter(utf16);

        if (c == '\n' || (c == '\r' && (it == e || *it != '\n' || (++it, true))))
        {
            ++end.line;
            it = utils::utf8_iterator(it.base(), counter());
            utils::utf8_next(it, lexing::default_ictl.continuation - 1, e);
        }
    }

    m_comments.emplace_back(rp.adjust_range({ start, end }));

    return m_comments;
}

macro_preprocessor_result new_parser::macro_preprocessor(
    std::string_view text, const position p, size_t logical_column, semantics::range_provider& rp, bool reparse)
{
    assert(logical_column > 0 && p.column > 0);

    clear();

    auto lineno = p.line;

    utils::utf8_iterator it(text.cbegin(), counter(p.column, logical_column));
    const auto e = text.cend();

    skip_spaces(it, e, lineno);

    const position op_start(lineno, it.counter(utf16));
    const size_t new_logical_column = it.counter(utf32);

    if (it == e)
        return { .line_range = rp.adjust_range(range(p, op_start)), .line_logical_column = new_logical_column };

    if (reparse == (it != text.cbegin()))
    {
        // TODO: diag (un)expected spaces
        return { .line_range = rp.adjust_range(range(p, op_start)), .line_logical_column = new_logical_column };
    }

    // return tmp == 'O' || tmp == 'S' || tmp == 'I' || tmp == 'L' || tmp == 'T';
    // return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '=' || c == '$' || c == '_' || c == '#' || c ==
    // '@';

    while (it != e)
    {
        const auto c = *it;
        ++it;
        if (it.counter(utf32) >= lexing::default_ictl.end && c != ' ') [[unlikely]]
        {
            while (it != e)
            {
                const auto c = *it;
                ++it;

                if (c == '\n' || (c == '\r' && (it == e || *it != '\n' || (++it, true))))
                {
                    ++lineno;
                    it = utils::utf8_iterator(it.base(), counter());
                    utils::utf8_next(it, lexing::default_ictl.continuation - 1, e);
                }
            }
        }

        switch (c)
        {
            case ' ':
                // TODO: parse comment
                break;

            case ',':
                // push ranges
                break;

            case '&':
                break;

            case '\'':
                break;

            case 'O':
            case 'S':
            case 'I':
            case 'L':
            case 'T':
            case 'o':
            case 's':
            case 'i':
            case 'l':
            case 't':
                if (it != e && *it == '\'')
                {
                    const auto n = std::next(it);
                    if (n != e && parser_impl::can_attribute_consume(*n))
                    {
                        // consuming attributes
                    }
                }
                break;

            case '\n':
                ++lineno;
                it = utils::utf8_iterator(it.base(), counter());
                utils::utf8_next(it, lexing::default_ictl.continuation - 1, e);
                break;

            case '\r':
                if (it != e && *it == '\n')
                    ++it;
                ++lineno;
                it = utils::utf8_iterator(it.base(), counter());
                utils::utf8_next(it, lexing::default_ictl.continuation - 1, e);
                break;
        }
    }

    return {
        .text = m_text,
        .text_ranges = m_text_ranges,
        .total_op_range =
            rp.adjust_range(m_text_ranges.empty() ? range(op_start) : range(op_start, m_text_ranges.back().end)),
        .remarks = m_comments,

        //.line_range = ,
        .line_logical_column = new_logical_column,
    };
}

new_parser::new_parser(context::hlasm_context& ctx) noexcept
    : m_ctx(&ctx)
{}

} // namespace hlasm_plugin::parser_library::parsing
