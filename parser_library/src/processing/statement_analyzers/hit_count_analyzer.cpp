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
#include <iterator>
#include <variant>

#include "context/hlasm_context.h"
#include "processing/statement.h"

namespace hlasm_plugin::parser_library::processing {

hit_count_analyzer::hit_count_analyzer(context::hlasm_context& ctx)
    : m_ctx(ctx)
{}

namespace {
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
        return std::make_pair(r.start.line, r.start.line + source.end_index - source.begin_index - 1);
    }

    return std::make_pair(r.start.line, r.end.line);
}
} // namespace

void hit_count_analyzer::update_hc_detail(hit_count_detail& hc_detail,
    stmt_lines_range lines_range,
    bool increase_hit_count,
    const std::optional<context::id_index>& macro_affiliation)
{
    hc_detail.has_sections |= !m_ctx.ord_ctx.sections().empty();
    hc_detail.hits.add(lines_range, increase_hit_count, true, macro_affiliation);
}

hit_count_detail& hit_count_analyzer::get_hc_detail_reference(const utils::resource::resource_location& rl)
{
    return m_hit_count.hc_map.try_emplace(rl).first->second;
}

hit_count_analyzer::statement_type hit_count_analyzer::get_stmt_type(const semantics::core_statement& statement)
{
    const auto instr_id = std::get_if<context::id_index>(&statement.instruction_ref().value);

    statement_type cur_stmt_type = m_next_stmt_type;
    if (instr_id)
    {
        if (*instr_id == context::id_storage::well_known::MACRO)
            cur_stmt_type = statement_type::MACRO_INIT;
        else if (!m_nest_macro_names.empty() && *instr_id == context::id_storage::well_known::MEND)
            cur_stmt_type = statement_type::MACRO_END;
    }

    switch (cur_stmt_type)
    {
        // TODO think about marking special macro statements somehow (in order not to recreate these state machines
        // again and again)
        using enum statement_type;
        case MACRO_INIT:
            m_next_stmt_type = MACRO_NAME;
            break;

        case MACRO_NAME: {
            assert(instr_id);
            m_nest_macro_names.push(*instr_id);
            m_next_stmt_type = MACRO_BODY;
            break;
        }

        case MACRO_END:
            if (!m_nest_macro_names.empty())
                m_nest_macro_names.pop();

            if (m_nest_macro_names.empty())
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
    auto core_stmt_o = get_core_stmt(&statement);
    if (!core_stmt_o.has_value() || !core_stmt_o.value())
        return false;

    const auto& core_stmt = *core_stmt_o.value();

    auto stmt_type = get_stmt_type(core_stmt);
    if (stmt_type == statement_type::MACRO_INIT || stmt_type == statement_type::MACRO_END)
        return false;

    auto stmt_lines_range = get_stmt_lines_range(core_stmt, prov_kind, proc_kind, m_ctx);
    if (!stmt_lines_range)
        return false;

    auto rl = m_ctx.current_statement_location(proc_kind != processing_kind::LOOKAHEAD).resource_loc;

    switch (stmt_type)
    {
        using enum statement_type;
        case REGULAR:
            update_hc_detail(get_hc_detail_reference(rl),
                *stmt_lines_range,
                proc_kind == processing::processing_kind::ORDINARY,
                std::nullopt);

            if (auto macro_id = m_ctx.current_macro_id(); !macro_id.empty())
            {
                auto& item = m_hit_count.macro_details_map[macro_id];
                item.used = true;
                item.rl = std::move(rl);
            }

            break;

        case MACRO_BODY:
            assert(!m_nest_macro_names.empty());
            update_hc_detail(get_hc_detail_reference(rl), *stmt_lines_range, false, m_nest_macro_names.top());

            break;

        case MACRO_NAME: {
            assert(!m_nest_macro_names.empty());
            auto& item = m_hit_count.macro_details_map[m_nest_macro_names.top()];
            item.macro_name_line = stmt_lines_range->first;
            item.rl = std::move(rl);

            break;
        }

        default:
            assert(false);
            break;
    }

    return false;
}

void hit_count_analyzer::analyze_aread_line(
    const utils::resource::resource_location& rl, size_t lineno, std::string_view)
{
    update_hc_detail(get_hc_detail_reference(rl), std::make_pair(lineno, lineno), true, std::nullopt);
}

} // namespace hlasm_plugin::parser_library::processing
