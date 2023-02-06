/*
 * Copyright (c) 2023 Broadcom.
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

#include "hit_count_analyzer.h"

#include <cassert>
#include <optional>

#include "context/id_storage.h"
#include "processing/error_statement.h"
#include "processing/statement.h"

namespace hlasm_plugin::parser_library::processing {

hit_count_analyzer::hit_count_analyzer(context::hlasm_context& ctx)
    : m_ctx(ctx)
{}

namespace {
std::optional<range> get_range(
    const context::hlasm_statement* stmt, statement_provider_kind prov_kind, context::hlasm_context& ctx)
{
    if (!stmt)
        return std::nullopt;

    const auto adj_range = [prov_kind, &ctx](range r) {
        if (prov_kind == statement_provider_kind::OPEN)
            r.end.line = ctx.current_source().end_index - 1;
        r.end.column = 72;
        return r;
    };

    if (stmt->kind == context::statement_kind::RESOLVED)
    {
        if (auto resolved = stmt->access_resolved(); resolved)
            return adj_range(resolved->stmt_range_ref());
    }
    else if (stmt->kind == context::statement_kind::DEFERRED)
    {
        if (auto deferred = stmt->access_deferred(); deferred)
            return adj_range(deferred->stmt_range_ref());
    }
    else
    {
        if (auto err_stmt = dynamic_cast<const error_statement*>(stmt); err_stmt)
            return adj_range(range(err_stmt->statement_position()));
    }

    return std::nullopt;
}

size_t& emplace_hit_count(
    hit_count_map& hc_map, const utils::resource::resource_location& rl, range r, bool is_external_macro)
{
    auto map_emplacer = []<typename VALUE>(auto& m, const auto& k) {
        if (auto m_it = m.find(k); m_it != m.end())
            return m_it;

        return m.try_emplace(k, VALUE {}).first;
    };

    auto& stmt_hc_details = map_emplacer.template operator()<stmt_hit_count_details>(hc_map, rl)->second;
    auto& [stmt_r, stmt_count] =
        map_emplacer.template operator()<hit_count_details>(stmt_hc_details.stmt_hc_map, r.start.line)->second;

    stmt_hc_details.is_external_macro &= is_external_macro;
    stmt_r = std::move(r);
    return stmt_count;
}
} // namespace

hit_count_analyzer::statement_type hit_count_analyzer::get_stmt_type(const context::hlasm_statement& statement)
{
    if (auto res_stmt = statement.access_resolved(); res_stmt)
    {
        switch (m_stmt_type)
        {
            case hlasm_plugin::parser_library::processing::hit_count_analyzer::statement_type::REGULAR:
                if (res_stmt->opcode_ref().value == context::id_storage::well_known::MACRO)
                {
                    if (++m_in_macro_def == 1)
                        m_stmt_type = statement_type::MACRO_INIT;
                }
                break;
            case hlasm_plugin::parser_library::processing::hit_count_analyzer::statement_type::MACRO_INIT:
                m_stmt_type = statement_type::MACRO_NAME;
                break;
            case hlasm_plugin::parser_library::processing::hit_count_analyzer::statement_type::MACRO_NAME:
                m_stmt_type = statement_type::MACRO_BODY;
                break;
            case hlasm_plugin::parser_library::processing::hit_count_analyzer::statement_type::MACRO_BODY:
                if (res_stmt->opcode_ref().value == context::id_storage::well_known::MEND)
                {
                    if (--m_in_macro_def == 0)
                        return std::exchange(m_stmt_type, statement_type::REGULAR);
                }
                break;
            default:
                break;
        }
    }

    return m_stmt_type;
}

void hit_count_analyzer::analyze(
    const context::hlasm_statement& statement, statement_provider_kind prov_kind, processing_kind proc_kind, bool)
{
    bool is_external_macro = false;

    if (auto stmt_type = get_stmt_type(statement); stmt_type == statement_type::MACRO_BODY)
        is_external_macro = true;
    else if (stmt_type != statement_type::REGULAR)
        return;

    auto stmt_range = get_range(&statement, prov_kind, m_ctx);
    if (!stmt_range)
        return;

    assert(m_ctx.processing_stack().frame().resource_loc);

    auto& count = emplace_hit_count(
        m_hit_counts, *m_ctx.processing_stack().frame().resource_loc, std::move(*stmt_range), is_external_macro);

    if (proc_kind == processing::processing_kind::ORDINARY)
        count++;
}
} // namespace hlasm_plugin::parser_library::processing