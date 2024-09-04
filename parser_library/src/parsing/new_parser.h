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

#include "range.h"

namespace hlasm_plugin::parser_library::context {
class hlasm_context;
}

namespace hlasm_plugin::parser_library::semantics {
struct range_provider;
}

namespace hlasm_plugin::parser_library::parsing {
struct macro_preprocessor_result
{
    std::string_view text;
    std::span<range> text_ranges;
    range total_op_range;
    std::span<range> remarks;

    range line_range;
    size_t line_logical_column;
};

class new_parser
{
    std::vector<range> m_comments;
    std::vector<range> m_text_ranges;
    std::string m_text;

    context::hlasm_context* m_ctx;

    void clear();

public:
    std::span<range> noop_operands(
        std::string_view text, position p, size_t logical_column, semantics::range_provider& rp);

    macro_preprocessor_result macro_preprocessor(
        std::string_view text, position p, size_t logical_column, semantics::range_provider& rp, bool reparse);

    explicit new_parser(context::hlasm_context& ctx) noexcept;
};

} // namespace hlasm_plugin::parser_library::parsing

#endif
