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

#ifndef HLASMPLUGIN_PARSERLIBRARY_CHECKING_MACHINE_CHECK_H
#define HLASMPLUGIN_PARSERLIBRARY_CHECKING_MACHINE_CHECK_H

#include <optional>

#include "diagnostic_op.h"
#include "instructions/instruction.h"
#include "range.h"

namespace hlasm_plugin::parser_library::checking {
enum class address_state : unsigned char
{
    EMPTY,
    RES_VALID,
    RES_INVALID,
    UNRES
};

enum class operand_state : unsigned char
{
    // D
    SIMPLE,
    // D(,B)
    FIRST_OMITTED,
    // D(X,B)
    PRESENT,
    // D(B)
    ONE_OP
};

class machine_operand final
{
public:
    machine_operand(const range& r);
    machine_operand(const range& r, int displacement);
    machine_operand(const range& r, address_state state, int displacement, int first, int second);
    machine_operand(
        const range& r, address_state state, int displacement, int first, int second, operand_state op_state);

    range operand_range;
    int displacement;
    int first_op;
    int second_op;
    address_state state;
    operand_state op_state;

    diagnostic_op get_simple_operand_expected(
        const instructions::machine_operand_format& op_format, std::string_view instr_name) const;

    static bool is_operand_corresponding(int operand, instructions::parameter param);
    static bool is_simple_operand(const instructions::machine_operand_format& operand);

    diagnostic_op get_first_parameter_error(
        instructions::machine_operand_type op_type, std::string_view instr_name, long long from, long long to) const;

    std::optional<diagnostic_op> check(
        instructions::machine_operand_format to_check, std::string_view instr_name) const;

    std::optional<diagnostic_op> check_simple(
        instructions::machine_operand_format to_check, std::string_view instr_name) const;

    bool is_length_corresponding(int param_value, int length_size) const;
};

} // namespace hlasm_plugin::parser_library::checking

#endif
