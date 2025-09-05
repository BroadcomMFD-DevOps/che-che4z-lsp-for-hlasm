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

#include "members_statement_provider.h"

#include "context/hlasm_context.h"
#include "library_info_transitional.h"
#include "utils/projectors.h"

namespace hlasm_plugin::parser_library::processing {

namespace {
statement_provider_kind translate_to_statement_provider_kind(members_statement_provider_kind kind)
{
    switch (kind)
    {
        case members_statement_provider_kind::MACRO:
            return statement_provider_kind::MACRO;
        case members_statement_provider_kind::COPY:
            return statement_provider_kind::COPY;
    }
}
} // namespace

members_statement_provider_kind members_statement_provider::kind() const noexcept
{
    switch (statement_provider::kind)
    {
        case statement_provider_kind::MACRO:
            return members_statement_provider_kind::MACRO;
        case statement_provider_kind::COPY:
            return members_statement_provider_kind::COPY;
        default:
            utils::insist_fail("members_statement_provider::kind()"); // c++23 std::unreachable();
    }
}

members_statement_provider::members_statement_provider(members_statement_provider_kind kind,
    const analyzing_context& ctx,
    statement_fields_parser& parser,
    parse_lib_provider& lib_provider,
    processing::processing_state_listener& listener,
    diagnostic_op_consumer& diag_consumer)
    : statement_provider(translate_to_statement_provider_kind(kind))
    , m_ctx(ctx)
    , m_parser(parser)
    , m_lib_provider(lib_provider)
    , m_listener(listener)
    , m_diagnoser(diag_consumer)
{}

statement_cache* members_statement_provider::get_next_macro()
{
    auto& invo = m_ctx.hlasm_ctx->scope_stack().back().this_macro;
    assert(invo);

    invo->current_statement.value += !m_resolved_instruction.has_value();
    if (invo->current_statement.value == invo->cached_definition.size())
    {
        m_resolved_instruction.reset();
        m_ctx.hlasm_ctx->leave_macro();
        return nullptr;
    }

    return &invo->cached_definition.at(invo->current_statement.value);
}

statement_cache* members_statement_provider::get_next_copy()
{
    // LIFETIME: copy stack should not move even if source stack changes
    // due to std::vector iterator invalidation rules for move
    auto& invo = m_ctx.hlasm_ctx->current_copy_stack().back();

    invo.current_statement.value += !m_resolved_instruction.has_value();
    if (invo.current_statement.value == invo.cached_definition()->size())
    {
        m_resolved_instruction.reset();
        m_ctx.hlasm_ctx->leave_copy_member();
        return nullptr;
    }

    return &invo.cached_definition()->at(invo.current_statement.value);
}

statement_cache* members_statement_provider::get_next()
{
    switch (kind())
    {
        case members_statement_provider_kind::MACRO:
            return get_next_macro();
        case members_statement_provider_kind::COPY:
            return get_next_copy();
    }
}

std::pair<context::shared_stmt_ptr, processing_status> members_statement_provider::get_next(
    const statement_processor& processor)
{
    if (finished())
        throw std::runtime_error("provider already finished");

    auto* const cache = get_next();

    if (!cache)
        return {};

    auto resolved_instruction = std::exchange(m_resolved_instruction, {});

    if (processor.kind == processing_kind::ORDINARY)
    {
        if (const auto* instr = retrieve_instruction(*cache))
        {
            if (try_trigger_attribute_lookahead(*instr,
                    { *m_ctx.hlasm_ctx, library_info_transitional(m_lib_provider), drop_diagnostic_op },
                    m_listener,
                    std::move(lookahead_references)))
                return {};
        }
    }

    std::pair<context::shared_stmt_ptr, processing_status> result;

    const auto& stmt_candiate = cache->get_base();
    switch (stmt_candiate->kind)
    {
        case context::statement_kind::RESOLVED:
            result.first = stmt_candiate;
            result.second = cache->get_base_status();
            break;
        case context::statement_kind::DEFERRED: {
            const auto& current_instr = stmt_candiate->access_deferred()->instruction;
            if (!resolved_instruction.has_value())
                resolved_instruction.emplace(processor.resolve_instruction(current_instr));
            auto proc_status_o = processor.get_processing_status(*resolved_instruction, current_instr.field_range);
            if (!proc_status_o.has_value())
            {
                m_resolved_instruction.emplace(std::move(*resolved_instruction));
                return {};
            }
            if (proc_status_o->first.form != processing_form::DEFERRED)
                result.first = preprocess_deferred(processor, *cache, *proc_status_o, stmt_candiate);
            else
                result.first = stmt_candiate;
            result.second = *proc_status_o;
            break;
        }
        case context::statement_kind::ERROR:
            result.first = stmt_candiate;
            break;
        default:
            break;
    }

    if (processor.kind == processing_kind::ORDINARY
        && try_trigger_attribute_lookahead(*result.first,
            { *m_ctx.hlasm_ctx, library_info_transitional(m_lib_provider), drop_diagnostic_op },
            m_listener,
            std::move(lookahead_references)))
        return {};

    return result;
}

bool members_statement_provider::finished() const
{
    switch (kind())
    {
        case members_statement_provider_kind::MACRO:
            return m_ctx.hlasm_ctx->scope_stack().size() == 1;

        case members_statement_provider_kind::COPY: {
            const auto& current_stack = m_ctx.hlasm_ctx->current_copy_stack();
            return current_stack.empty() || (m_ctx.hlasm_ctx->in_opencode() && current_stack.back().suspended());
        }
    }
}

const semantics::instruction_si* members_statement_provider::retrieve_instruction(const statement_cache& cache) const
{
    const auto& stmt = cache.get_base();
    switch (stmt->kind)
    {
        case context::statement_kind::RESOLVED:
            return &stmt->access_resolved()->instruction_ref();
        case context::statement_kind::DEFERRED:
            return &stmt->access_deferred()->instruction;
        case context::statement_kind::ERROR:
            return nullptr;
        default:
            return nullptr;
    }
}

// structure holding deferred statement that is now complete
struct statement_si_defer_done final
{
    statement_si_defer_done(std::shared_ptr<const semantics::deferred_statement> deferred_stmt,
        semantics::operands_si operands,
        semantics::remarks_si remarks,
        std::vector<semantics::literal_si> collected_literals)
        : deferred_stmt(std::move(deferred_stmt))
        , operands(std::move(operands))
        , remarks(std::move(remarks))
        , collected_literals(std::move(collected_literals))
    {}

    std::shared_ptr<const semantics::deferred_statement> deferred_stmt;

    semantics::operands_si operands;
    semantics::remarks_si remarks;
    std::vector<semantics::literal_si> collected_literals;
};

const statement_cache::cached_statement_t& members_statement_provider::fill_cache(statement_cache& cache,
    std::shared_ptr<const semantics::deferred_statement> def_stmt,
    const processing_status& status)
{
    const bool no_operands = status.first.occurrence == operand_occurrence::ABSENT
        || status.first.form == processing_form::UNKNOWN || status.first.form == processing_form::IGNORED;

    const auto diags = kind() == members_statement_provider_kind::COPY || no_operands
        ? def_stmt->diagnostics_without_operands()
        : def_stmt->diagnostics();
    statement_cache::cached_statement_t reparsed_stmt {
        {},
        { diags.begin(), diags.end() },
    };
    const auto& def_ops = def_stmt->deferred_operands;

    if (no_operands)
    {
        semantics::operands_si op(def_ops.field_range, semantics::operand_list());
        semantics::remarks_si rem({});

        reparsed_stmt.stmt = std::make_shared<statement_si_defer_done>(
            std::move(def_stmt), std::move(op), std::move(rem), std::vector<semantics::literal_si>());
    }
    else
    {
        diagnostic_consumer_transform diag_consumer(
            [&reparsed_stmt](diagnostic_op diag) { reparsed_stmt.diags.push_back(std::move(diag)); });
        auto [op, rem, lits] = m_parser.parse_operand_field(def_ops.value,
            false,
            semantics::range_provider(def_ops.field_range, semantics::adjusting_state::NONE),
            def_ops.logical_column,
            status,
            diag_consumer);

        reparsed_stmt.stmt = std::make_shared<statement_si_defer_done>(
            std::move(def_stmt), std::move(op), std::move(rem), std::move(lits));
    }
    return cache.cache_.emplace_back(processing_status_cache_key(status), std::move(reparsed_stmt)).second;
}

struct members_statement_provider::deferred_statement_adapter final : public resolved_statement
{
    deferred_statement_adapter(std::shared_ptr<const statement_si_defer_done> base_stmt)
        : base_stmt(std::move(base_stmt))
    {}

    std::shared_ptr<const statement_si_defer_done> base_stmt;

    const range& stmt_range_ref() const override { return base_stmt->deferred_stmt->stmt_range; }
    const semantics::label_si& label_ref() const override { return base_stmt->deferred_stmt->label; }
    const semantics::instruction_si& instruction_ref() const override { return base_stmt->deferred_stmt->instruction; }
    const semantics::operands_si& operands_ref() const override { return base_stmt->operands; }
    const semantics::remarks_si& remarks_ref() const override { return base_stmt->remarks; }
    std::span<const semantics::literal_si> literals() const override { return base_stmt->collected_literals; }
    std::span<const diagnostic_op> diagnostics() const override { return {}; }
};

context::shared_stmt_ptr members_statement_provider::preprocess_deferred(const statement_processor& processor,
    statement_cache& cache,
    const processing_status& status,
    context::shared_stmt_ptr base_stmt)
{
    const auto& def_stmt = *base_stmt->access_deferred();

    processing_status_cache_key key(status);

    const auto item = std::ranges::find(cache.cache_, key, utils::first_element);
    const auto item_end = std::ranges::end(cache.cache_);
    const auto& cache_item =
        item == item_end ? fill_cache(cache, { std::move(base_stmt), &def_stmt }, status) : item->second;

    if (processor.kind != processing_kind::LOOKAHEAD)
    {
        for (const diagnostic_op& diag : cache_item.diags)
            m_diagnoser.add_diagnostic(diag);
    }

    return std::make_shared<deferred_statement_adapter>(cache_item.stmt);
}

} // namespace hlasm_plugin::parser_library::processing
