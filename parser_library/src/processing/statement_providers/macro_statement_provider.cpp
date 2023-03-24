/*
 * Copyright (c) 2019 Broadcom.
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

#include "macro_statement_provider.h"

#include "processing/processing_manager.h"

namespace hlasm_plugin::parser_library::processing {

macro_statement_provider::macro_statement_provider(analyzing_context ctx,
    statement_fields_parser& parser,
    workspaces::parse_lib_provider& lib_provider,
    processing::processing_state_listener& listener,
    diagnostic_op_consumer& diag_consumer)
    : members_statement_provider(
        statement_provider_kind::MACRO, std::move(ctx), parser, lib_provider, listener, diag_consumer)
{}

bool macro_statement_provider::finished() const { return ctx.hlasm_ctx->scope_stack().size() == 1; }

std::pair<context::statement_cache*, std::optional<std::optional<context::id_index>>>
macro_statement_provider::get_next()
{
    auto& invo = ctx.hlasm_ctx->scope_stack().back().this_macro;
    assert(invo);

    invo->current_statement += !resolved_instruction.has_value();
    if (invo->current_statement == invo->cached_definition.size())
    {
        resolved_instruction.reset();
        ctx.hlasm_ctx->leave_macro();
        return {};
    }

    return { &invo->cached_definition[invo->current_statement], std::exchange(resolved_instruction, {}) };
}

std::vector<diagnostic_op> macro_statement_provider::filter_cached_diagnostics(
    const semantics::deferred_statement& stmt) const
{
    auto diags = stmt.diagnostics();
    return std::vector<diagnostic_op>(diags.begin(), diags.end());
}

} // namespace hlasm_plugin::parser_library::processing
