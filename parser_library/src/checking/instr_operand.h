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

#ifndef HLASMPLUGIN_PARSERLIBRARY_INSTR_OPERAND_H
#define HLASMPLUGIN_PARSERLIBRARY_INSTR_OPERAND_H

#include <assert.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "diagnostic_op.h"
#include "operand.h"

namespace hlasm_plugin::parser_library::instructions {
struct parameter;
struct machine_operand_format;
enum class machine_operand_type : uint8_t;
} // namespace hlasm_plugin::parser_library::instructions

namespace hlasm_plugin::parser_library::checking {
class data_def_type;

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

// extended class representing complex operands
// contains vector of all parameters of operand - for example FLAG, COMPAT, OPTABLE...
class complex_operand final : public asm_operand
{
public:
    std::string operand_identifier;
    std::vector<std::unique_ptr<asm_operand>> operand_parameters;

    complex_operand();
    complex_operand(std::string operand_identifier, std::vector<std::unique_ptr<asm_operand>> operand_params);
};

constexpr bool is_size_corresponding_signed(int operand, int size)
{
    auto boundary = 1ll << (size - 1);
    return operand < boundary && operand >= -boundary;
}

constexpr bool is_size_corresponding_unsigned(int operand, int size)
{
    return operand >= 0 && operand <= (1ll << size) - 1;
}


// Abstract class that represents a machine operand suitable for checking.
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

// class that represents both a simple operand both in assembler and machine instructions
class one_operand final : public asm_operand
{
public:
    // the string value of the operand
    std::string operand_identifier;
    int value;
    bool is_default;

    one_operand();

    one_operand(std::string operand_identifier, int value);

    one_operand(std::string operand_identifier);

    one_operand(int value);

    one_operand(std::string operand_identifier, range range);

    one_operand(int value, range range);

    one_operand(const one_operand& op);
};

class empty_operand final : public asm_operand
{
public:
    empty_operand() = default;
    explicit empty_operand(range r);
};

} // namespace hlasm_plugin::parser_library::checking

#endif
