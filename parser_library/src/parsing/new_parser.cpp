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
#include "semantics/range_provider.h"
#include "utils/unicode_text.h"

namespace hlasm_plugin::parser_library::parsing {

void new_parser::clear() { m_comments.clear(); }

std::span<range> new_parser::noop_operands(std::string_view text, position p, size_t logical_column)
{
    assert(logical_column > 0 && p.column > 0);

    clear();

    if (text.empty())
        return {};

    auto lineno = p.line;

    using counter = utils::utf8_multicounter<utils::utf8_utf16_counter, utils::utf8_utf32_counter>;
    static constexpr auto utf16 = utils::counter_index<0>;
    static constexpr auto utf32 = utils::counter_index<1>;

    utils::utf8_iterator it(text.cbegin(), counter(p.column, logical_column));
    const auto e = text.cend();

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

    if (it == e)
        return {};

    position start(lineno, it.counter(utf16));

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

    m_comments.emplace_back(start, position(lineno, it.counter(utf16)));

    return m_comments;
}

new_parser::new_parser(context::hlasm_context& ctx) noexcept
    : m_ctx(&ctx)
{}

} // namespace hlasm_plugin::parser_library::parsing
