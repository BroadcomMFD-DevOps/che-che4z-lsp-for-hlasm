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

std::optional<range> get_range(const context::hlasm_statement* stmt, processing::processing_kind proc_kind)
{
    if (!stmt)
        return std::nullopt;

    const auto r_helper = [&proc_kind](const range& r) -> std::optional<range> {
        return (proc_kind == processing::processing_kind::ORDINARY && r.start == r.end && r.start.column == 0)
            ? std::optional<range>()
            : r;
    };

    if (stmt->kind == context::statement_kind::RESOLVED)
    {
        if (auto resolved = stmt->access_resolved(); resolved)
            return r_helper(resolved->stmt_range_ref());
    }
    else if (stmt->kind == context::statement_kind::DEFERRED)
    {
        if (auto deferred = stmt->access_deferred(); deferred)
            return r_helper(deferred->stmt_range_ref());
    }
    else
    {
        auto err_stmt = dynamic_cast<const error_statement*>(stmt);
        if (!err_stmt)
            return std::nullopt;

        auto r = range(err_stmt->statement_position());
        r.end.column = 72;

        return r;
    }
}

template<typename DEFINITION>
void insert_external_definitions(DEFINITION definition,
    std::string_view lib_name,
    processing_kind proc_kind,
    hit_count_map& hc_map,
    std::unordered_set<std::string_view>& processed_libs)
{
    if (!definition || processed_libs.contains(lib_name))
        return;

    processed_libs.emplace(lib_name);

    for (const auto& stmt : definition->cached_definition)
    {
        if (const auto stmt_range = get_range(stmt.get_base().get(), proc_kind); stmt_range)
            emplace_hit_count(hc_map, definition->definition_location.resource_loc, std::move(*stmt_range));
    }
}
} // namespace

void hit_count_analyzer::analyze(
    const context::hlasm_statement& statement, statement_provider_kind prov_kind, processing_kind proc_kind, bool)
{
    if (statement.kind == context::statement_kind::DEFERRED || proc_kind == processing::processing_kind::MACRO
        || proc_kind == processing::processing_kind::COPY)
        return;

    auto stmt_range = get_range(&statement, proc_kind);
    if (!stmt_range)
        return;

    const auto& frame = m_ctx.processing_stack().frame();

    if (prov_kind == statement_provider_kind::MACRO)
        insert_external_definitions(m_ctx.get_macro_definition(frame.member_name),
            frame.member_name.to_string_view(),
            proc_kind,
            m_hit_counts,
            m_processed_members);
    else if (prov_kind == statement_provider_kind::COPY)
        insert_external_definitions(m_ctx.get_copy_member(frame.member_name),
            frame.member_name.to_string_view(),
            proc_kind,
            m_hit_counts,
            m_processed_members);
    else
    {
        stmt_range->end.line = m_ctx.current_source().end_index - 1;
        stmt_range->end.column = 72;
    }

    auto& count = emplace_hit_count(m_hit_counts, *frame.resource_loc, std::move(*stmt_range));

    if (proc_kind == processing::processing_kind::ORDINARY)
        count++;
}
} // namespace hlasm_plugin::parser_library::processing