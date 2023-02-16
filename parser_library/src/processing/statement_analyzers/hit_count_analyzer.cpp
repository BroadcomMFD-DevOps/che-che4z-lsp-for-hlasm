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

#include <algorithm>
#include <cassert>
#include <iterator>
#include <optional>
#include <variant>

#include "context/hlasm_context.h"
#include "processing/statement.h"

namespace hlasm_plugin::parser_library::processing {

hit_count_analyzer::hit_count_analyzer(context::hlasm_context& ctx)
    : m_ctx(ctx)
{}

namespace {
constexpr auto stmt_lines_compacter = [](const range& r) { return std::make_pair(r.start.line, r.end.line); };

constexpr auto get_core_stmt =
    [](const context::hlasm_statement* stmt) -> std::optional<const semantics::core_statement*> {
    if (!stmt)
        return std::nullopt;

    if (stmt->kind == context::statement_kind::RESOLVED)
        return stmt->access_resolved();
    else if (stmt->kind == context::statement_kind::DEFERRED)
        return stmt->access_deferred();

    return std::nullopt;
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
        return std::make_pair(r.start.line, r.end.line + source.end_index - source.begin_index - 1);
    }

    return stmt_lines_compacter(r);
}

void update_hc_details(
    hit_count_details& hc_details, stmt_lines_range lines_range, bool can_be_external_macro, bool increase_hit_count)
{
    const auto& [start_line, end_line] = lines_range;

    hc_details.is_external_macro &= can_be_external_macro;
    hc_details.max_lineno = std::max(hc_details.max_lineno, start_line);

    auto& line_hits = hc_details.line_hits;
    if (line_hits.size() <= end_line)
        line_hits.resize(2 * end_line + 1000);

    for (auto i = start_line; i <= end_line; ++i)
    {
        auto& line_hit = line_hits[i];
        line_hit.contains_statement = true;
        if (increase_hit_count)
            line_hit.count++;
    }
}
} // namespace

hit_count_details& hit_count_analyzer::get_hc_details_reference(utils::resource::resource_location rl)
{
    return m_hit_counts.try_emplace(std::move(rl)).first->second;
}

void hit_count_analyzer::emplace_macro_header_definitions(const context::id_index& id)
{
    if (auto map_it = m_macro_header_definitions.find(id);
        map_it != m_macro_header_definitions.end() && !map_it->second.used && map_it->second.defined)
    {
        auto& mac_header_details = map_it->second;
        update_hc_details(get_hc_details_reference(mac_header_details.rl), mac_header_details.init_line_r, true, true);
        update_hc_details(get_hc_details_reference(mac_header_details.rl), mac_header_details.name_line_r, true, true);
        update_hc_details(get_hc_details_reference(mac_header_details.rl), mac_header_details.mend_line_r, true, true);

        mac_header_details.used = true;
    }
}

hit_count_analyzer::statement_type hit_count_analyzer::get_stmt_type(
    const semantics::core_statement& statement, processing_kind proc_kind)
{
    const auto& r = statement.stmt_range_ref();
    const auto instr_id = std::get_if<context::id_index>(&statement.instruction_ref().value);

    statement_type cur_stmt_type = m_next_stmt_type;
    if (instr_id)
    {
        if (*instr_id == context::id_storage::well_known::MACRO)
            cur_stmt_type = statement_type::MACRO_INIT;
        else if (*instr_id == context::id_storage::well_known::MEND)
            cur_stmt_type = statement_type::MACRO_END;
    }

    switch (cur_stmt_type)
    {
        // TODO think about marking special macro statements somehow (in order not to recreate these state machines
        // again and again)
        using enum statement_type;
        case MACRO_INIT:
            m_last_macro_init_line_ranges = stmt_lines_compacter(r);
            m_next_stmt_type = MACRO_NAME;
            if (!m_nest_macro_names.empty())
                cur_stmt_type = REGULAR;
            break;

        case MACRO_NAME: {
            assert(instr_id);

            m_nest_macro_names.push(*instr_id);
            m_macro_header_definitions.try_emplace(*instr_id,
                macro_header_definition_details {
                    m_ctx.current_statement_resource_location(proc_kind != processing_kind::LOOKAHEAD),
                    m_last_macro_init_line_ranges,
                    stmt_lines_compacter(r) });

            m_next_stmt_type = MACRO_BODY;
            if (m_nest_macro_names.size() > 1)
                cur_stmt_type = REGULAR;
            break;
        }

        case MACRO_END:
            if (!m_nest_macro_names.empty())
            {
                assert(m_macro_header_definitions.contains(m_nest_macro_names.top()));

                auto& header_def = m_macro_header_definitions[m_nest_macro_names.top()];
                header_def.mend_line_r = stmt_lines_compacter(r);
                header_def.defined = true;

                m_nest_macro_names.pop();
            }

            if (m_nest_macro_names.empty())
            {
                m_next_stmt_type = REGULAR;
                cur_stmt_type = MACRO_END;
            }
            else
            {
                m_next_stmt_type = MACRO_BODY;
                cur_stmt_type = MACRO_BODY;
            }
            break;

        default:
            break;
    }

    return cur_stmt_type;
}

void hit_count_analyzer::analyze(
    const context::hlasm_statement& statement, statement_provider_kind prov_kind, processing_kind proc_kind, bool)
{
    auto core_stmt_o = get_core_stmt(&statement);
    if (!core_stmt_o.has_value() || !core_stmt_o.value())
        return;

    const auto& core_stmt = *core_stmt_o.value();

    bool can_be_external_macro = false;

    switch (get_stmt_type(core_stmt, proc_kind))
    {
        using enum statement_type;
        case REGULAR:
            break;

        case MACRO_BODY:
            can_be_external_macro = true;
            break;

        default:
            return;
    }

    auto stmt_lines_range = get_stmt_lines_range(core_stmt, prov_kind, proc_kind, m_ctx);
    if (!stmt_lines_range)
        return;

    if (auto macro_id = m_ctx.current_macro_id(); !macro_id.empty())
        emplace_macro_header_definitions(macro_id);

    update_hc_details(
        get_hc_details_reference(m_ctx.current_statement_resource_location(proc_kind != processing_kind::LOOKAHEAD)),
        *stmt_lines_range,
        can_be_external_macro,
        proc_kind == processing::processing_kind::ORDINARY);
}
} // namespace hlasm_plugin::parser_library::processing
