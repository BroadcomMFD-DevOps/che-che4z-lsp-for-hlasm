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
std::optional<range> get_range(const context::hlasm_statement* stmt, processing::processing_kind proc_kind)
{
    if (!stmt)
        return std::nullopt;

    if (stmt->kind == context::statement_kind::RESOLVED)
    {
        if (auto resolved = stmt->access_resolved(); resolved)
            return resolved->stmt_range_ref();
    }
    else if (stmt->kind == context::statement_kind::DEFERRED)
    {
        if (auto deferred = stmt->access_deferred(); deferred)
            return deferred->stmt_range_ref();
    }
    else
    {
        auto err_stmt = dynamic_cast<const error_statement*>(stmt);
        if (!err_stmt)
            return std::nullopt;

        return range(err_stmt->statement_position());
    }
}

size_t& emplace_hit_count(hit_count_map& hc_map, const utils::resource::resource_location& rl, range r)
{
    auto map_emplacer = []<typename VALUE>(auto& m, const auto& k) {
        if (auto m_it = m.find(k); m_it != m.end())
            return m_it;

        return m.try_emplace(k, VALUE {}).first;
    };

    auto& stmt_hc_m = map_emplacer.template operator()<stmt_hit_count_map>(hc_map, rl)->second;
    auto& [stmt_r, stmt_count] = map_emplacer.template operator()<hit_count_details>(stmt_hc_m, r.start.line)->second;

    stmt_r = std::move(r);
    return stmt_count;
}
} // namespace


void hit_count_analyzer::analyze(
    const context::hlasm_statement& statement, statement_provider_kind prov_kind, processing_kind proc_kind, bool)
{
    if (auto res_stmt = statement.access_resolved();
        res_stmt && res_stmt->opcode_ref().value == context::id_storage::well_known::MACRO)
    {
        m_expecting_macro = true;
        return;
    }

    if (m_expecting_macro)
    {
        m_expecting_macro = false;
        return;
    }

    auto stmt_range = get_range(&statement, proc_kind);
    if (!stmt_range)
        return;

    if (prov_kind == statement_provider_kind::OPEN)
        stmt_range->end.line = m_ctx.current_source().end_index - 1;
    stmt_range->end.column = 72;

    assert(m_ctx.processing_stack().frame().resource_loc);

    auto& count =
        emplace_hit_count(m_hit_counts, *m_ctx.processing_stack().frame().resource_loc, std::move(*stmt_range));

    if (proc_kind == processing::processing_kind::ORDINARY)
        count++;
}
} // namespace hlasm_plugin::parser_library::processing