/*
 * Copyright (c) 2021 Broadcom.
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

#include "data_def_postponed_statement.h"

#include <limits>

#include "context/using.h"
#include "semantics/operand_impls.h"

namespace hlasm_plugin::parser_library::processing {

template<checking::data_instr_type instr_type>
data_def_postponed_statement<instr_type>::data_def_postponed_statement(rebuilt_statement&& stmt,
    context::processing_stack_t stmt_location_stack,
    std::vector<data_def_dependency<instr_type>> dependencies)
    : postponed_statement_impl(std::move(stmt), std::move(stmt_location_stack))
    , m_dependencies(std::move(dependencies))
{}

// Inherited via resolvable
template<checking::data_instr_type instr_type>
context::dependency_collector data_def_dependency<instr_type>::get_dependencies(
    context::dependency_solver& _solver) const
{
    replace_loctr_dependency_solver solver(_solver, &m_loctr);
    context::dependency_collector deps;
    for (auto it = m_begin; it != m_end; ++it)
    {
        const auto& op = *it;
        if (op->type == semantics::operand_type::EMPTY)
            continue;
        deps.merge(op->access_data_def()->get_length_dependencies(solver));
    }
    return deps;
}

template<checking::data_instr_type instr_type>
int32_t data_def_dependency<instr_type>::get_operands_length(const semantics::operand_ptr* b,
    const semantics::operand_ptr* e,
    context::dependency_solver& _solver,
    diagnostic_op_consumer& diags,
    const context::address& loctr)
{
    uint64_t byte_length = 0;
    uint64_t operands_bit_length = 0;
    std::optional<context::address> adjusted_loctr;

    replace_loctr_dependency_solver solver(_solver, &loctr);

    constexpr auto round_up_bytes = [](uint64_t& v, uint64_t bytes) { v = checking::round_up(v, bytes * 8); };

    for (auto it = b; it != e; ++it)
    {
        const auto& op = *it;
        if (op->type == semantics::operand_type::EMPTY)
            continue;

        if (auto dd = op->access_data_def()->value.get();
            dd->length_type != expressions::data_definition::length_type::BIT)
        {
            // align to whole byte
            round_up_bytes(operands_bit_length, 1);

            // enforce data def alignment
            round_up_bytes(operands_bit_length, dd->get_alignment().boundary);
        }
        const auto byte_diff = operands_bit_length / 8 - byte_length;
        if (byte_diff)
        {
            if (!adjusted_loctr)
            {
                solver.loctr = &adjusted_loctr.emplace(loctr);
            }
            *adjusted_loctr += byte_diff;
            byte_length += byte_diff;
        }
        long long len = op->access_data_def()->evaluate_total_length(solver, instr_type, diags);

        if (len < 0)
            return 0;

        operands_bit_length += len;
    }

    // align to whole byte
    round_up_bytes(operands_bit_length, 1);

    // returns the length in bytes
    uint64_t len = operands_bit_length / 8;

    if (len > std::numeric_limits<int32_t>::max())
        return 0;
    else
        return (int32_t)len;
}

template<checking::data_instr_type instr_type>
context::symbol_value data_def_dependency<instr_type>::resolve(context::dependency_solver& solver) const
{
    return get_operands_length(m_begin, m_end, solver, drop_diagnostic_op, m_loctr);
}

template class data_def_postponed_statement<checking::data_instr_type::DC>;
template class data_def_postponed_statement<checking::data_instr_type::DS>;
template class data_def_dependency<checking::data_instr_type::DC>;
template class data_def_dependency<checking::data_instr_type::DS>;

const context::address* replace_loctr_dependency_solver::get_loctr() const
{
    if (loctr)
        return loctr;
    return dependency_solver_redirect::get_loctr();
}

} // namespace hlasm_plugin::parser_library::processing
