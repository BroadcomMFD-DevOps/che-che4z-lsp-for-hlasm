/*
 * Copyright (c) 2022 Broadcom.
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

#include "preprocessor_utils.h"

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>

#include "lexing/logical_line.h"
#include "utils/string_operations.h"

namespace hlasm_plugin::parser_library::processing {
namespace {
size_t get_quoted_string_end(std::string_view s)
{
    auto closing_quote = std::string_view::npos;

    s.remove_prefix(1);
    while (closing_quote == std::string_view::npos)
    {
        closing_quote = s.find_first_of("'");

        if (closing_quote == std::string_view::npos || closing_quote == s.length() - 1
            || s[closing_quote + 1] != '\'') // ignore double quotes
            break;

        s = s.substr(closing_quote + 1);
        closing_quote = std::string_view::npos;
    }

    return closing_quote;
}

size_t get_argument_length(std::string_view s)
{
    auto string_end_pos = std::string_view::npos;
    if (auto string_start_pos = s.find_first_of("'"); string_start_pos != std::string_view::npos)
        string_end_pos = get_quoted_string_end(s);

    return string_end_pos == std::string_view::npos ? s.length() : s.find_first_of(")", string_end_pos) + 1;
}

std::string_view extract_operand_and_argument(std::string_view s)
{
    static constexpr std::string_view separators = " ,";

    auto separator_pos = s.find_first_of(separators);
    if (separator_pos == std::string_view::npos)
        return s;

    auto parenthesis = s.find_first_of("(");
    if (parenthesis == std::string_view::npos)
        return s.substr(0, separator_pos);

    if (parenthesis > separator_pos)
        if (auto prev_char = s.find_last_not_of(separators, parenthesis - 1); prev_char > separator_pos)
            return s.substr(0, separator_pos);

    return s.substr(0, get_argument_length(s));
}

std::pair<std::string_view, size_t> remove_separators(std::string_view s)
{
    auto trimmed = hlasm_plugin::utils::trim_left(s);
    if (!s.empty() && s.front() == ',')
    {
        s.remove_prefix(1);
        trimmed += 1;
    }

    return { s, trimmed };
}
} // namespace

void fill_operands_list(std::string_view operands,
    size_t op_column_start,
    const semantics::range_provider& rp,
    std::vector<semantics::preproc_details::name_range>& operand_list)
{
    auto lineno = rp.original_range.start.line;

    while (!operands.empty())
    {
        size_t trimmed = 0;
        std::tie(operands, trimmed) = remove_separators(operands);
        if (operands.empty())
            break;

        op_column_start += trimmed;

        auto operand_view = extract_operand_and_argument(operands);
        std::string operand;
        operand.reserve(operand_view.length());
        std::remove_copy_if(operand_view.begin(), operand_view.end(), std::back_inserter(operand), [](unsigned char c) {
            return utils::isblank32(c);
        });

        operand_list.emplace_back(semantics::preproc_details::name_range { std::move(operand),
            rp.adjust_range(range(
                (position(lineno, op_column_start)), (position(lineno, op_column_start + operand_view.length())))) });

        operands.remove_prefix(operand_view.length());
        op_column_start += operand_view.length();
    }
}

namespace {
template<typename ITERATOR>
semantics::preproc_details::name_range get_stmt_part_name_range(
    const typename stmt_part_details<ITERATOR>::it_string_tuple& detail,
    const ITERATOR& it_start,
    const semantics::range_provider& rp)
{
    auto lineno = rp.original_range.start.line;
    auto first_dist = std::distance(it_start, detail.it_s);

    return semantics::preproc_details::name_range(
        { detail.name ? std::move(*detail.name) : std::string(detail.it_s, detail.it_e),
            rp.adjust_range(range(position(lineno, first_dist),
                position(lineno, std::distance(detail.it_s, detail.it_e) + first_dist))) });
}
} // namespace

template<typename ITERATOR>
std::shared_ptr<semantics::preprocessor_statement_si> get_preproc_statement(
    const stmt_part_details<ITERATOR>& stmt_parts, size_t lineno, size_t continue_column)
{
    semantics::preproc_details details;

    details.stmt_r = range(
        { lineno, 0 }, { lineno, static_cast<size_t>(std::distance(stmt_parts.stmt.it_s, stmt_parts.stmt.it_e)) });
    auto rp = semantics::range_provider(details.stmt_r, semantics::adjusting_state::MACRO_REPARSE, continue_column);

    if (stmt_parts.label && stmt_parts.label->it_s != stmt_parts.label->it_e)
        details.label = get_stmt_part_name_range<ITERATOR>(*stmt_parts.label, stmt_parts.stmt.it_s, rp);

    // Let's store the complete instruction range and only the last word of the instruction as it is unique
    if (stmt_parts.instruction.it_s != stmt_parts.instruction.it_e)
        details.instruction = get_stmt_part_name_range<ITERATOR>(stmt_parts.instruction, stmt_parts.stmt.it_s, rp);

    if (stmt_parts.operands && stmt_parts.operands->it_s != stmt_parts.operands->it_e)
        fill_operands_list(get_stmt_part_name_range<ITERATOR>(*stmt_parts.operands, stmt_parts.stmt.it_s, rp).name,
            std::distance(stmt_parts.stmt.it_s, stmt_parts.operands->it_s),
            rp,
            details.operands);

    if (stmt_parts.remarks && stmt_parts.remarks->it_s != stmt_parts.remarks->it_e)
        details.remarks.emplace_back(
            get_stmt_part_name_range<ITERATOR>(*stmt_parts.remarks, stmt_parts.stmt.it_s, rp).r);

    return std::make_shared<semantics::preprocessor_statement_si>(std::move(details), stmt_parts.copy_like);
}

template std::shared_ptr<semantics::preprocessor_statement_si>
get_preproc_statement<lexing::logical_line::const_iterator>(
    const stmt_part_details<lexing::logical_line::const_iterator>& stmt_parts, size_t lineno, size_t continue_column);

template std::shared_ptr<semantics::preprocessor_statement_si> get_preproc_statement<std::string_view::iterator>(
    const stmt_part_details<std::string_view::iterator>& stmt_parts, size_t lineno, size_t continue_column);

} // namespace hlasm_plugin::parser_library::processing
