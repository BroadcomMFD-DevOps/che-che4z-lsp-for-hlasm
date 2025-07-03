/*
 * Copyright (c) 2025 Broadcom.
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

#include "machine_check.h"

#include "instr_operand.h"

namespace hlasm_plugin::parser_library::checking {

namespace {
bool check_value_parity(int operand, instructions::even_odd_register reg)
{
    using enum instructions::even_odd_register;
    switch (reg)
    {
        case NONE:
            return true;
        case ODD:
            return !!(operand & 1);
        case EVEN:
            return !(operand & 1);
    }
}
} // namespace

bool machine_operand::is_operand_corresponding(int operand, instructions::parameter param)
{
    if (param.is_signed && is_size_corresponding_signed(operand, param.size))
        return true;
    else if (!param.is_signed && is_size_corresponding_unsigned(operand, param.size))
        return true;
    return false;
}

bool machine_operand::is_simple_operand(const instructions::machine_operand_format& operand)
{
    using enum instructions::machine_operand_type;
    return (operand.first.is_signed == false && operand.first.size == 0 && operand.second.is_signed == false
        && operand.second.size == 0 && operand.first.type == NONE && operand.second.type == NONE);
}

machine_operand::machine_operand(const range& r)
    : operand_range(r)
    , displacement(0)
    , first_op(0)
    , second_op(0)
    , state(address_state::EMPTY)
    , op_state(operand_state::SIMPLE)
{}

machine_operand::machine_operand(const range& r, int displacement)
    : machine_operand(r, address_state::UNRES, displacement, 0, 0, operand_state::SIMPLE)
{}

machine_operand::machine_operand(const range& r, address_state state, int displacement, int first, int second)
    : operand_range(r)
    , displacement(displacement)
    , first_op(first)
    , second_op(second)
    , state(state)
    , op_state(operand_state::PRESENT) {};

machine_operand::machine_operand(
    const range& r, address_state state, int displacement, int first, int second, operand_state op_state)
    : operand_range(r)
    , displacement(displacement)
    , first_op(first)
    , second_op(second)
    , state(state)
    , op_state(op_state) {};

diagnostic_op machine_operand::get_first_parameter_error(
    instructions::machine_operand_type op_type, std::string_view instr_name, long long from, long long to) const
{
    using enum instructions::machine_operand_type;
    switch (op_type)
    {
        case LENGTH: // D(L,B)
            return diagnostic_op::error_M132(instr_name, from, to, operand_range);
        case IDX_REG: // D(X,B)
            return diagnostic_op::error_M135(instr_name, from, to, operand_range);
        case REG: // D(R,B)
            return diagnostic_op::error_M133(instr_name, from, to, operand_range);
        case VEC_REG: // D(V,B)
            return diagnostic_op::error_M134(instr_name, from, to, operand_range);
    }
    assert(false);
    return diagnostic_op::error_I999(instr_name, operand_range);
}

std::optional<diagnostic_op> machine_operand::check(
    instructions::machine_operand_format to_check, std::string_view instr_name) const
{
    if (state == address_state::EMPTY)
        return diagnostic_op::error_M003(instr_name, operand_range);

    if (op_state == operand_state::SIMPLE)
        return check_simple(to_check, instr_name);

    if (is_simple_operand(to_check))
        return get_simple_operand_expected(to_check, instr_name); // operand must be simple

    if (state == address_state::RES_VALID)
        return std::nullopt;
    else if (state == address_state::RES_INVALID)
        return diagnostic_op::error_M010(instr_name, operand_range);

    // check displacement
    if (!is_operand_corresponding(displacement, to_check.identifier))
    {
        if (to_check.identifier.is_signed)
        {
            auto boundary = 1ll << (to_check.identifier.size - 1);
            return diagnostic_op::error_M130(instr_name, -boundary, boundary - 1, operand_range);
        }
        else
            return diagnostic_op::error_M130(instr_name, 0, (1ll << to_check.identifier.size) - 1, operand_range);
    }

    // check the D(B) operand format
    if (to_check.first.is_empty())
    {
        if (op_state != operand_state::ONE_OP)
        {
            // error, cannot be present
            return diagnostic_op::error_M104(instr_name, operand_range);
        }
        // check the only parameter, in all cases this is the base parameter
        if (!is_operand_corresponding(first_op, to_check.second))
        {
            return diagnostic_op::error_M131(instr_name, operand_range);
        }
        return std::nullopt;
    }

    // therefore a D(X,B) format expected, can be specified either by D(X,B), D(X) or D(,B)

    // first check the base register in case it is specified
    if (op_state == operand_state::FIRST_OMITTED || op_state == operand_state::PRESENT)
    {
        if (!is_operand_corresponding(second_op, to_check.second))
        {
            return diagnostic_op::error_M131(instr_name, operand_range);
        }
    }

    // base is now checked, now check the first parameter, which is specified either in D(X,B) or D(X)
    if (op_state != operand_state::FIRST_OMITTED)
    {
        // check whether value is corresponding
        using enum instructions::machine_operand_type;
        // length parameters can have +1 values specified
        if (to_check.first.type == LENGTH && !is_length_corresponding(first_op, to_check.first.size))
        {
            return get_first_parameter_error(to_check.first.type, instr_name, 0, 1ll << to_check.first.size);
        }
        if (to_check.first.type != LENGTH && !is_operand_corresponding(first_op, to_check.first))
        {
            assert(!to_check.first.is_signed);
            return get_first_parameter_error(to_check.first.type, instr_name, 0, (1ll << to_check.first.size) - 1);
        }
    }
    return std::nullopt;
}

bool hlasm_plugin::parser_library::checking::machine_operand::is_length_corresponding(
    int param_value, int length_size) const
{
    auto boundary = 1ll << length_size;
    if (param_value > boundary || param_value < 0)
        return false;
    return true;
};

diagnostic_op machine_operand::get_simple_operand_expected(
    const instructions::machine_operand_format& op_format, std::string_view instr_name) const
{
    using enum instructions::machine_operand_type;
    switch (op_format.identifier.type)
    {
        case REG: // R
            return diagnostic_op::error_M110(instr_name, operand_range);
        case MASK: // M
            return diagnostic_op::error_M111(instr_name, operand_range);
        case IMM: // I
            return diagnostic_op::error_M112(instr_name, operand_range);
        case VEC_REG: // V
            return diagnostic_op::error_M114(instr_name, operand_range);
        case RELOC_IMM: // RI
            return diagnostic_op::error_M113(instr_name, operand_range);
    }
    assert(false);
    return diagnostic_op::error_I999(instr_name, operand_range);
}

std::optional<diagnostic_op> machine_operand::check_simple(
    instructions::machine_operand_format to_check, std::string_view instr_name) const
{
    if (!is_simple_operand(to_check))
    {
        // therefore it is an address operand, but represented only by a single value

        // check only the displacement
        if (is_operand_corresponding(displacement, to_check.identifier))
            return std::nullopt;

        if (to_check.identifier.is_signed)
        {
            auto boundary = 1ll << (to_check.identifier.size - 1);
            return diagnostic_op::error_M130(instr_name, -boundary, boundary - 1, operand_range);
        }
        else
            return diagnostic_op::error_M130(instr_name, 0, (1ll << to_check.identifier.size) - 1, operand_range);
    }

    using enum instructions::machine_operand_type;
    // it is a simple operand
    if (to_check.identifier.is_signed && !is_size_corresponding_signed(displacement, to_check.identifier.size))
    {
        const auto boundary = 1ll << (to_check.identifier.size - 1);
        switch (to_check.identifier.type)
        {
            case IMM:
                return diagnostic_op::warn_M137(instr_name, -boundary, boundary - 1, operand_range);
            case RELOC_IMM:
                return diagnostic_op::error_M123(instr_name, -boundary, boundary - 1, operand_range);
            default:
                assert(false);
        }
    }
    if (!to_check.identifier.is_signed
        && (!is_size_corresponding_unsigned(displacement, to_check.identifier.size)
            || !check_value_parity(displacement, to_check.identifier.evenodd)
            || displacement < to_check.identifier.min_register))
    {
        const auto boundary = (1ll << to_check.identifier.size) - 1;
        static constexpr std::string_view reg_qual[] = { "", "odd", "even" };
        switch (to_check.identifier.type)
        {
            case REG:
                return diagnostic_op::error_M120(instr_name,
                    operand_range,
                    reg_qual[(int)to_check.identifier.evenodd],
                    to_check.identifier.min_register);
            case MASK:
                return diagnostic_op::error_M121(instr_name, operand_range);
            case IMM:
                return diagnostic_op::warn_M137(instr_name, 0, boundary, operand_range);
            case VEC_REG:
                return diagnostic_op::error_M124(instr_name, operand_range);
            default:
                assert(false);
        }
    }
    return std::nullopt;
}


} // namespace hlasm_plugin::parser_library::checking
