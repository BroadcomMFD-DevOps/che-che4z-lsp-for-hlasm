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

#include "lexing/logical_line.h"
#include "semantics/statement.h"

namespace hlasm_plugin::parser_library::processing {

namespace {
std::vector<semantics::preproc_details::name_range> get_operands_list(
    std::string_view operands, size_t column_offset, size_t lineno)
{
    std::vector<semantics::preproc_details::name_range> operand_list;

    while (operands.size())
    {
        auto pos = operands.find_first_of(" (),'");

        if (pos == std::string_view::npos)
        {
            operand_list.emplace_back(semantics::preproc_details::name_range { std::string(operands),
                range((position(lineno, column_offset)), (position(lineno, column_offset + operands.length()))) });
            break;
        }

        operand_list.emplace_back(semantics::preproc_details::name_range { std::string(operands.substr(0, pos)),
            range((position(lineno, column_offset)), (position(lineno, column_offset + pos))) });

        operands.remove_prefix(pos);
        column_offset += pos;

        pos = operands.find_first_not_of(" (),'");
        if (pos == std::string_view::npos)
            break;

        operands.remove_prefix(pos);
        column_offset += pos;
    }

    return operand_list;
}

template<typename ITERATOR>
semantics::preproc_details::name_range get_stmt_part_name_range(
    const std::match_results<ITERATOR>& matches, size_t index, size_t line_no)
{
    semantics::preproc_details::name_range nr;

    if (index < matches.size() && matches[index].length())
    {
        nr.name = std::string_view(std::to_address(&*matches[index].first), matches[index].length());
        nr.r = range(position(line_no, std::distance(matches[0].first, matches[index].first)),
            position(line_no, std::distance(matches[0].first, matches[index].second)));
    }

    return nr;
}
} // namespace

template<typename PREPROC_STATEMENT, typename ITERATOR>
std::shared_ptr<PREPROC_STATEMENT> get_preproc_statement(
    const std::match_results<ITERATOR>& matches, stmt_part_ids ids, size_t lineno)
{
    if (!matches.size())
        return nullptr;

    semantics::preproc_details details;

    details.stmt_r = range({ lineno, 0 }, { lineno, matches[0].str().length() });

    if (ids.label)
        details.label = get_stmt_part_name_range<ITERATOR>(matches, *ids.label, lineno);

    if (ids.instruction.size())
    {
        // Let's store the complete instruction range and only the last word of the instruction as it is unique
        details.instruction = get_stmt_part_name_range<ITERATOR>(matches, ids.instruction.back(), lineno);
        details.instruction.r.start =
            get_stmt_part_name_range<ITERATOR>(matches, ids.instruction.front(), lineno).r.start;
    }

    if (ids.operands < matches.size() && matches[ids.operands].length())
    {
        auto [ops_text, op_range] = get_stmt_part_name_range<ITERATOR>(matches, ids.operands, lineno);
        details.operands.first = get_operands_list(ops_text, op_range.start.column, lineno);
        details.operands.second = std::move(op_range);
    }

    if (ids.remarks && *ids.remarks < matches.size() && matches[*ids.remarks].length())
    {
        details.remarks.second = get_stmt_part_name_range<ITERATOR>(matches, *ids.remarks, lineno).r;

        details.remarks.first.emplace_back(details.remarks.second);
    }

    return std::make_shared<PREPROC_STATEMENT>(std::move(details));
}

template std::shared_ptr<semantics::endevor_statement_si>
get_preproc_statement<semantics::endevor_statement_si, std::string_view::iterator>(
    const std::match_results<std::string_view::iterator>& matches, stmt_part_ids ids, size_t lineno);

template std::shared_ptr<semantics::cics_statement_si>
get_preproc_statement<semantics::cics_statement_si, lexing::logical_line::const_iterator>(
    const std::match_results<lexing::logical_line::const_iterator>& matches, stmt_part_ids ids, size_t lineno);


} // namespace hlasm_plugin::parser_library::processing