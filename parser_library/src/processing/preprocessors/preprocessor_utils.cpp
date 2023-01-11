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
    const std::sub_match<ITERATOR>& submatch, const ITERATOR& it_start, const semantics::range_provider& rp)
{
    auto lineno = rp.original_range.start.line;
    auto first_dist = std::distance(it_start, submatch.first);

    return semantics::preproc_details::name_range({ submatch.str(),
        rp.adjust_range(range(position(lineno, first_dist),
            position(lineno, std::distance(submatch.first, submatch.second) + first_dist))) });
}

template<typename ITERATOR>
semantics::preproc_details::name_range get_stmt_part_name_range(
    const std::match_results<ITERATOR>& matches, size_t index, const semantics::range_provider& rp)
{
    return (index < matches.size() && matches[index].length())
        ? get_stmt_part_name_range<ITERATOR>(matches[index], matches[0].first, rp)
        : semantics::preproc_details::name_range();
}

template<typename ITERATOR>
void handle_suffix(stmt_part_ids::suffix_type type,
    const std::match_results<ITERATOR>& matches,
    const semantics::range_provider& rp,
    semantics::preproc_details& details)
{
    if (!matches.suffix().length())
        return;

    const auto& it_start = matches[0].first;
    const auto& suffix = matches.suffix();

    if (type == stmt_part_ids::suffix_type::OPERANDS)
        fill_operands_list(get_stmt_part_name_range<ITERATOR>(suffix, it_start, rp).name,
            std::distance(it_start, suffix.first),
            rp,
            details.operands);
    else if (type == stmt_part_ids::suffix_type::REMARKS)
        details.remarks.emplace_back(get_stmt_part_name_range<ITERATOR>(suffix, it_start, rp).r);
}
} // namespace

template<typename PREPROC_STATEMENT, typename ITERATOR>
std::shared_ptr<PREPROC_STATEMENT> get_preproc_statement(
    const std::match_results<ITERATOR>& matches, const stmt_part_ids& ids, size_t lineno, size_t continue_column)
{
    assert(!matches.empty() && ids.operands < matches.size() && (!ids.remarks || *ids.remarks < matches.size()));

    semantics::preproc_details details;

    details.stmt_r = range({ lineno, 0 }, { lineno, static_cast<size_t>(matches[0].length()) });
    auto rp = semantics::range_provider(details.stmt_r, semantics::adjusting_state::MACRO_REPARSE, continue_column);

    if (ids.label)
        details.label = get_stmt_part_name_range<ITERATOR>(matches, *ids.label, rp);

    if (ids.instruction.size())
    {
        // Let's store the complete instruction range and only the last word of the instruction as it is unique
        details.instruction = get_stmt_part_name_range<ITERATOR>(matches, ids.instruction.back(), rp);
        details.instruction.r.start = get_stmt_part_name_range<ITERATOR>(matches, ids.instruction.front(), rp).r.start;
    }

    if (ids.operands && matches[*ids.operands].length())
        fill_operands_list(get_stmt_part_name_range<ITERATOR>(matches, *ids.operands, rp).name,
            std::distance(matches[0].first, matches[*ids.operands].first),
            rp,
            details.operands);

    if (ids.remarks && matches[*ids.remarks].length())
        details.remarks.emplace_back(get_stmt_part_name_range<ITERATOR>(matches, *ids.remarks, rp).r);

    handle_suffix<ITERATOR>(ids.suffix, matches, rp, details);

    return std::make_shared<PREPROC_STATEMENT>(std::move(details));
}

template std::shared_ptr<semantics::preprocessor_statement_si>
get_preproc_statement<semantics::preprocessor_statement_si, lexing::logical_line::const_iterator>(
    const std::match_results<lexing::logical_line::const_iterator>& matches,
    const stmt_part_ids& ids,
    size_t lineno,
    size_t continuation_column);

template std::shared_ptr<semantics::endevor_statement_si>
get_preproc_statement<semantics::endevor_statement_si, std::string_view::iterator>(
    const std::match_results<std::string_view::iterator>& matches,
    const stmt_part_ids& ids,
    size_t lineno,
    size_t continuation_column);
} // namespace hlasm_plugin::parser_library::processing
