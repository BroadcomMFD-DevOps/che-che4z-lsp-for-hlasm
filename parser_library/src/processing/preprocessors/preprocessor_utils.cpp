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

#include "lexing/logical_line.h"
#include "semantics/statement.h"
#include "utils/string_operations.h"

namespace hlasm_plugin::parser_library::processing {
namespace {
std::pair<std::string, size_t> extract_resulting_string(std::string_view s, size_t end_position)
{
    auto extracted_string = s.substr(0, end_position);
    auto ret_val = std::string(extracted_string);
    ret_val.erase(remove_if(ret_val.begin(), ret_val.end(), isspace), ret_val.end());
    return { ret_val, extracted_string.length() };
}

size_t get_quoted_string_end(std::string_view s)
{
    auto closing_quote = std::string_view::npos;

    s.remove_prefix(1);
    while (closing_quote == std::string_view::npos)
    {
        closing_quote = s.find_first_of("'");

        if (closing_quote == std::string_view::npos || closing_quote == s.length()
            || s[closing_quote + 1] != '\'') // ignore double quotes
            break;

        s = s.substr(closing_quote + 1);
        closing_quote = std::string_view::npos;
    }

    return closing_quote;
}

size_t get_argument_end(std::string_view s)
{
    auto string_end_pos = std::string_view::npos;
    if (auto string_start_pos = s.find_first_of("'"); string_start_pos != std::string_view::npos)
        string_end_pos = get_quoted_string_end(s);

    return string_end_pos == std::string_view::npos ? s.length() : s.find_first_of(")", string_end_pos) + 1;
}

std::pair<std::string, size_t> extract_operand_and_argument(std::string_view s)
{
    static const std::string separators = " ,";

    auto separator_pos = s.find_first_of(separators);
    if (separator_pos == std::string_view::npos)
        return extract_resulting_string(s, s.length());

    auto parenthesis = s.find_first_of("(");
    if (parenthesis == std::string_view::npos)
        return extract_resulting_string(s, separator_pos);

    if (parenthesis > separator_pos)
        if (auto prev_char = s.find_last_not_of(separators, parenthesis - 1); prev_char > separator_pos)
            return extract_resulting_string(s, separator_pos);

    return extract_resulting_string(s, get_argument_end(s));
}

std::pair<std::string_view, size_t> remove_separators(std::string_view s)
{
    size_t trimmed = 0;
    std::tie(s, trimmed) = hlasm_plugin::utils::trim_left(s);
    if (!s.empty() && s.front() == ',')
    {
        s.remove_prefix(1);
        trimmed += 1;
    }

    return { s, trimmed };
}
} // namespace

std::vector<semantics::preproc_details::name_range> get_operands_list(
    std::string_view operands, range r, const size_t continuation_column)
{
    std::vector<semantics::preproc_details::name_range> operand_list;
    const size_t lineno = r.start.line;
    size_t column_offset = r.start.column;
    auto rp = semantics::range_provider(std::move(r), semantics::adjusting_state::MACRO_REPARSE, continuation_column);

    while (!operands.empty())
    {
        size_t trimmed = 0;
        std::tie(operands, trimmed) = remove_separators(operands);
        if (operands.empty())
            break;

        column_offset += trimmed;
        auto [op, extracted_length] = extract_operand_and_argument(operands);

        operand_list.emplace_back(semantics::preproc_details::name_range { std::move(op),
            rp.adjust_range(
                range((position(lineno, column_offset)), (position(lineno, column_offset + extracted_length)))) });

        operands.remove_prefix(extracted_length);
        column_offset += extracted_length;
    }

    return operand_list;
}

namespace {
template<typename ITERATOR>
semantics::preproc_details::name_range get_stmt_part_name_range(
    const std::match_results<ITERATOR>& matches, size_t index, size_t line_no, const semantics::range_provider& rp)
{
    semantics::preproc_details::name_range nr;

    if (index < matches.size() && matches[index].length())
    {
        nr.name = matches[index].str();
        nr.r = rp.adjust_range(range(position(line_no, std::distance(matches[0].first, matches[index].first)),
            position(line_no, std::distance(matches[0].first, matches[index].second))));
    }

    return nr;
}
} // namespace

template<typename PREPROC_STATEMENT, typename ITERATOR>
std::shared_ptr<PREPROC_STATEMENT> get_preproc_statement(
    const std::match_results<ITERATOR>& matches, stmt_part_ids ids, size_t lineno, size_t continuation_column)
{
    if (!matches.size() || ids.operands >= matches.size() || (ids.remarks && *ids.remarks >= matches.size()))
        return nullptr;

    semantics::preproc_details details;

    details.stmt_r = range({ lineno, 0 }, { lineno, matches[0].str().length() });
    auto rp = semantics::range_provider(details.stmt_r, semantics::adjusting_state::MACRO_REPARSE, continuation_column);

    if (ids.label)
        details.label = get_stmt_part_name_range<ITERATOR>(matches, *ids.label, lineno, rp);

    if (ids.instruction.size())
    {
        // Let's store the complete instruction range and only the last word of the instruction as it is unique
        details.instruction = get_stmt_part_name_range<ITERATOR>(matches, ids.instruction.back(), lineno, rp);
        details.instruction.r.start =
            get_stmt_part_name_range<ITERATOR>(matches, ids.instruction.front(), lineno, rp).r.start;
    }

    if (matches[ids.operands].length())
    {
        auto [ops_text, ops_range] = get_stmt_part_name_range<ITERATOR>(matches, ids.operands, lineno, rp);
        details.operands.items = get_operands_list(ops_text, ops_range, continuation_column);
        details.operands.overall_r = std::move(ops_range);
    }

    if (ids.remarks && matches[*ids.remarks].length())
    {
        details.remarks.overall_r = get_stmt_part_name_range<ITERATOR>(matches, *ids.remarks, lineno, rp).r;
        details.remarks.items.emplace_back(details.remarks.overall_r);
    }

    return std::make_shared<PREPROC_STATEMENT>(std::move(details));
}

template std::shared_ptr<semantics::endevor_statement_si>
get_preproc_statement<semantics::endevor_statement_si, std::string_view::iterator>(
    const std::match_results<std::string_view::iterator>& matches,
    stmt_part_ids ids,
    size_t lineno,
    size_t continuation_column);

template std::shared_ptr<semantics::cics_statement_si>
get_preproc_statement<semantics::cics_statement_si, lexing::logical_line::const_iterator>(
    const std::match_results<lexing::logical_line::const_iterator>& matches,
    stmt_part_ids ids,
    size_t lineno,
    size_t continuation_column);


} // namespace hlasm_plugin::parser_library::processing