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

#include "preprocessor.h"

#include "lexing/logical_line.h"
#include "semantics/source_info_processor.h"
#include "semantics/statement.h"
#include "utils/unicode_text.h"

namespace hlasm_plugin::parser_library::processing {

preprocessor::line_iterator preprocessor::extract_nonempty_logical_line(
    lexing::logical_line& out, line_iterator it, line_iterator end, const lexing::logical_line_extractor_args& opts)
{
    out.clear();

    while (it != end)
    {
        auto text = it++->text();
        if (!append_to_logical_line(out, text, opts))
            break;
    }

    finish_logical_line(out, opts);

    return it;
}

bool preprocessor::is_continued(std::string_view s)
{
    const auto cont = utils::utf8_substr(s, lexing::default_ictl_copy.end, 1).str;
    return !cont.empty() && cont != " ";
}

void preprocessor::clear_statements() { m_statements.clear(); }

void preprocessor::set_statement(std::shared_ptr<semantics::preprocessor_statement_si> stmt)
{
    m_statements.emplace_back(std::move(stmt));
}

void preprocessor::set_statements(std::vector<std::shared_ptr<semantics::preprocessor_statement_si>> stmts)
{
    m_statements.insert(
        m_statements.end(), std::make_move_iterator(stmts.begin()), std::make_move_iterator(stmts.end()));
}

std::vector<std::shared_ptr<semantics::preprocessor_statement_si>> preprocessor::take_statements()
{
    return std::move(m_statements);
}

void preprocessor::do_highlighting(
    const semantics::preprocessor_statement_si& stmt, semantics::source_info_processor& src_proc) const
{
    const auto& details = stmt.m_details;

    if (!details.label.name.empty())
        src_proc.add_hl_symbol(token_info(details.label.r, semantics::hl_scopes::label));

    src_proc.add_hl_symbol(token_info(details.instruction.r, semantics::hl_scopes::instruction));

    if (!details.operands.items.empty())
        src_proc.add_hl_symbol(token_info(details.operands.overall_r, semantics::hl_scopes::operand));

    if (!details.remarks.items.empty())
        src_proc.add_hl_symbol(token_info(details.remarks.overall_r, semantics::hl_scopes::remark));
}

} // namespace hlasm_plugin::parser_library::processing
