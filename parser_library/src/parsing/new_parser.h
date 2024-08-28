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

#ifndef HLASMPLUGIN_PARSERLIBRARY_NEW_PARSER_H
#define HLASMPLUGIN_PARSERLIBRARY_NEW_PARSER_H

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "lexing/logical_line.h"
#include "range.h"
#include "utils/unicode_text.h"

namespace hlasm_plugin::parser_library::context {
class hlasm_context;
}

namespace hlasm_plugin::parser_library::semantics {
struct range_provider;
}

namespace hlasm_plugin::parser_library::parsing {

class new_parser
{
    lexing::logical_line<utils::utf8_iterator<std::string::const_iterator, utils::utf8_utf16_counter>> m_ll;

    std::vector<range> m_comments;

    context::hlasm_context* m_ctx;

    void clear();

public:
    std::span<range> noop_operands(std::string_view text, position p, size_t logical_column);

    explicit new_parser(context::hlasm_context& ctx) noexcept;
};

} // namespace hlasm_plugin::parser_library::parsing

#endif
