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

#include "processing/statement.h"

namespace hlasm_plugin::parser_library::processing {

hit_count_analyzer::hit_count_analyzer(context::hlasm_context& ctx)
    : m_ctx(ctx)
{}

void hit_count_analyzer::analyze(const context::hlasm_statement& statement,
    statement_provider_kind prov_kind,
    processing_kind proc_kind,
    bool evaluated_model)
{
    if (!evaluated_model && prov_kind == statement_provider_kind::MACRO)
        return; // we already stopped on the model itself

    // Continue only for ordinary processing kind (i.e. when the statement is executed, skip
    // lookahead and copy/macro definitions)
    // if (proc_kind != processing::processing_kind::ORDINARY)
    //    return;

    const auto* resolved_stmt = statement.access_resolved();
    if (!resolved_stmt)
        return;

    // Continue only for non-empty statements
    // if (resolved_stmt->opcode_ref().value.empty())
    //    return;

    auto stack_node = m_ctx.processing_stack();
    auto stack = m_ctx.processing_stack_details();
    auto rl = stack_node.frame().resource_loc;

    const auto get_range = [&resolved_stmt, &stack_node]() {
        return resolved_stmt->opcode_ref().value.empty() ? resolved_stmt->stmt_range_ref()
                                                         : resolved_stmt->stmt_range_ref();
    };

    hit_counts_entry hc_entry { *rl, resolved_stmt->stmt_range_ref().start.line };
    if (!m_hit_counts.contains(hc_entry))
        m_hit_counts[hc_entry] = hit_count_details { get_range(), 0 };

    (proc_kind == processing::processing_kind::ORDINARY
        || (proc_kind == processing::processing_kind::MACRO && prov_kind == statement_provider_kind::OPEN))
        ? m_hit_counts[hc_entry].count++
        : m_hit_counts[hc_entry].count;
}

const hit_count_analyzer::hit_count_map& hit_count_analyzer::get_hit_counts() const { return m_hit_counts; }
} // namespace hlasm_plugin::parser_library::processing