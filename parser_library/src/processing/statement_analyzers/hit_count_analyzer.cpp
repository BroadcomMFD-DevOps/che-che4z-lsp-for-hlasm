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
std::optional<range> get_range(const context::hlasm_statement* stmt,
    statement_provider_kind prov_kind,
    processing_kind proc_kind,
    const context::hlasm_context& ctx)
{
    if (!stmt)
        return std::nullopt;

    const auto adj_range = [prov_kind, &proc_kind, &ctx](range r) -> std::optional<range> {
        if (proc_kind != processing_kind::LOOKAHEAD && r.start == r.end)
            return std::nullopt;

        if (prov_kind == statement_provider_kind::OPEN)
            r.end.line += ctx.current_source().end_index - ctx.current_source().begin_index - 1;
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

    return std::nullopt;
}

size_t& emplace_hit_count(
    hit_count_map& hc_map, const utils::resource::resource_location& rl, range r, bool can_be_external_macro)
{
    auto& stmt_hc_details = hc_map.try_emplace(rl).first->second;
    auto& [stmt_r, stmt_count] = stmt_hc_details.stmt_hc_map.try_emplace(r.start.line).first->second;

    stmt_hc_details.is_external_macro &= can_be_external_macro;
    stmt_r = std::move(r);
    return stmt_count;
}
} // namespace

void hit_count_analyzer::emplace_macro_header_definitions(const context::id_index& id)
{
    if (auto map_it = m_macro_header_definitions.find(id.to_string()); map_it != m_macro_header_definitions.end())
    {
        auto& mac_header_details = map_it->second;
        emplace_hit_count(m_hit_counts, mac_header_details.rl, std::move(mac_header_details.init_r), true)++;
        emplace_hit_count(m_hit_counts, mac_header_details.rl, std::move(mac_header_details.name_r), true)++;

        m_macro_header_definitions.erase(map_it);
    }
}

hit_count_analyzer::statement_type hit_count_analyzer::get_stmt_type(const context::hlasm_statement& statement)
{
    auto res_stmt = statement.access_resolved();
    if (!res_stmt)
        return m_stmt_type;

    const auto macro_init_incrementer = [&nest_level = m_macro_nest_level](auto resolved_stmt) {
        if (resolved_stmt->opcode_ref().value != context::id_storage::well_known::MACRO)
            return false;

        ++nest_level;
        return true;
    };

    static constexpr auto range_adj = [](range r) {
        r.end.column = 72;
        return r;
    };

    switch (m_stmt_type)
    {
        case hlasm_plugin::parser_library::processing::hit_count_analyzer::statement_type::REGULAR:
            if (macro_init_incrementer(res_stmt))
            {
                m_stmt_type = statement_type::MACRO_INIT;
                m_last_macro_init_r = res_stmt->stmt_range_ref();
            }
            break;

        case hlasm_plugin::parser_library::processing::hit_count_analyzer::statement_type::MACRO_INIT: {
            m_stmt_type = statement_type::MACRO_NAME;

            assert(m_ctx.processing_stack_top().resource_loc);

            m_macro_header_definitions.try_emplace(res_stmt->opcode_ref().value.to_string(),
                macro_header_definition_details { *m_ctx.processing_stack_top().resource_loc,
                    range_adj(std::move(m_last_macro_init_r)),
                    range_adj(res_stmt->stmt_range_ref()) });
            break;
        }

        case hlasm_plugin::parser_library::processing::hit_count_analyzer::statement_type::MACRO_NAME:
            m_stmt_type = statement_type::MACRO_BODY;
            macro_init_incrementer(res_stmt);
            break;

        case hlasm_plugin::parser_library::processing::hit_count_analyzer::statement_type::MACRO_BODY:
            if (res_stmt->opcode_ref().value == context::id_storage::well_known::MEND)
            {
                if (--m_macro_nest_level == 0)
                    return std::exchange(m_stmt_type, statement_type::REGULAR);
            }
            else
                macro_init_incrementer(res_stmt);
            break;

        default:
            break;
    }

    return m_stmt_type;
}

void hit_count_analyzer::analyze(
    const context::hlasm_statement& statement, statement_provider_kind prov_kind, processing_kind proc_kind, bool)
{
    bool can_be_external_macro = false;

    switch (get_stmt_type(statement))
    {
        using enum processing::hit_count_analyzer::statement_type;
        case REGULAR:
            break;

        case MACRO_BODY:
            can_be_external_macro = true;
            break;

        default:
            return;
    }

    auto stmt_range = get_range(&statement, prov_kind, proc_kind, m_ctx);
    if (!stmt_range)
        return;

    if (auto macro_invo = m_ctx.this_macro(); macro_invo)
        emplace_macro_header_definitions(macro_invo->id);

    assert(m_ctx.processing_stack_top().resource_loc);

    auto& count = emplace_hit_count(
        m_hit_counts, *m_ctx.processing_stack_top().resource_loc, std::move(*stmt_range), can_be_external_macro);

    if (proc_kind == processing::processing_kind::ORDINARY)
        count++;
}
} // namespace hlasm_plugin::parser_library::processing
