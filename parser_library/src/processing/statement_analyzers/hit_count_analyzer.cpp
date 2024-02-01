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
#include <variant>

#include "context/hlasm_context.h"
#include "processing/statement.h"
#include "semantics/statement.h"

namespace hlasm_plugin::parser_library::processing {

hit_count_analyzer::hit_count_analyzer(context::hlasm_context& ctx)
    : m_ctx(ctx)
{}

namespace {
constexpr auto get_core_stmt = [](const context::hlasm_statement* stmt) -> const semantics::core_statement* {
    if (!stmt)
        return nullptr;

    if (stmt->kind == context::statement_kind::RESOLVED)
        return stmt->access_resolved();
    else if (stmt->kind == context::statement_kind::DEFERRED)
        return stmt->access_deferred();

    return nullptr;
};

std::optional<stmt_lines_range> get_stmt_lines_range(const semantics::core_statement& statement,
    statement_provider_kind prov_kind,
    processing_kind proc_kind,
    const context::hlasm_context& ctx)
{
    const auto& r = statement.stmt_range_ref();

    if (proc_kind != processing_kind::LOOKAHEAD && r.start == r.end)
        return std::nullopt;

    if (prov_kind == statement_provider_kind::OPEN)
    {
        const auto& source = ctx.current_source();
        // TODO: look into the opencode range discrepancy
        return std::make_pair(r.start.line, r.start.line + source.end_index - source.begin_index - 1);
    }

    return std::make_pair(r.start.line, r.end.line);
}
} // namespace

const utils::resource::resource_location& hit_count_analyzer::get_current_stmt_rl(processing_kind proc_kind) const
{
    return m_ctx.current_statement_source(proc_kind != processing_kind::LOOKAHEAD);
}

hit_count_entry& hit_count_analyzer::get_hc_entry_reference(const utils::resource::resource_location& rl)
{
    return m_hit_count_map.try_emplace(rl).first->second;
}

hit_count_analyzer::statement_type hit_count_analyzer::get_stmt_type(const semantics::core_statement& statement)
{
    statement_type cur_stmt_type = m_next_stmt_type;
    if (const auto instr_id = std::get_if<context::id_index>(&statement.instruction_ref().value); instr_id)
    {
        if (*instr_id == context::id_storage::well_known::MACRO)
            cur_stmt_type = statement_type::MACRO_INIT;
        else if (m_macro_level > 0 && *instr_id == context::id_storage::well_known::MEND)
            cur_stmt_type = statement_type::MACRO_END;
    }

    switch (cur_stmt_type)
    {
        // TODO think about marking special macro statements somehow (in order not to recreate these state machines
        // again and again)
        using enum statement_type;
        case MACRO_INIT:
            m_macro_level++;
            m_next_stmt_type = MACRO_NAME;
            break;

        case MACRO_NAME:
            m_next_stmt_type = MACRO_BODY;
            break;

        case MACRO_END:
            if (m_macro_level > 0)
                m_macro_level--;

            if (m_macro_level == 0)
                m_next_stmt_type = REGULAR;
            else
                m_next_stmt_type = MACRO_BODY;
            break;

        default:
            break;
    }

    return cur_stmt_type;
}

bool hit_count_analyzer::analyze(
    const context::hlasm_statement& statement, statement_provider_kind prov_kind, processing_kind proc_kind, bool)
{
    auto core_stmt_ptr = get_core_stmt(&statement);
    if (!core_stmt_ptr)
        return false;

    const auto& core_stmt = *core_stmt_ptr;

    auto stmt_type = get_stmt_type(core_stmt);
    if (stmt_type != statement_type::REGULAR && stmt_type != statement_type::MACRO_BODY)
        return false;

    auto stmt_lines_range = get_stmt_lines_range(core_stmt, prov_kind, proc_kind, m_ctx);
    if (!stmt_lines_range)
        return false;

    switch (stmt_type)
    {
        using enum statement_type;
        case REGULAR: {
            auto& hc_ref = get_hc_entry_reference(get_current_stmt_rl(proc_kind));
            hc_ref.hits.add(*stmt_lines_range, proc_kind == processing::processing_kind::ORDINARY, false);

            if (auto mac_invo_loc = m_ctx.current_macro_definition_location(); mac_invo_loc)
                hc_ref.emplace_line(mac_invo_loc->pos.line);

            break;
        }

        case MACRO_BODY:
            get_hc_entry_reference(get_current_stmt_rl(proc_kind)).hits.add(*stmt_lines_range, false, true);
            break;

        default:
            assert(false);
            break;
    }

    return false;
}

void hit_count_analyzer::analyze_aread_line(
    const utils::resource::resource_location& rl, size_t lineno, std::string_view)
{
    get_hc_entry_reference(rl).hits.add(std::make_pair(lineno, lineno), true, false);
}

hit_count_map hit_count_analyzer::take_hit_count_map()
{
    auto has_sections = !m_ctx.ord_ctx.sections().empty();
    for (auto& [_, hc_entry] : m_hit_count_map)
        hc_entry.has_sections = has_sections;

    return std::move(m_hit_count_map);
}

} // namespace hlasm_plugin::parser_library::processing
