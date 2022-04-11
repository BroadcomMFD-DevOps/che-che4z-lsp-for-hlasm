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

#include "instruction.h"

#include <algorithm>
#include <limits>

using namespace hlasm_plugin::parser_library::context;
using namespace hlasm_plugin::parser_library::checking;
using namespace hlasm_plugin::parser_library;

namespace {
// Determines in which systems is the instruction supported
// First Byte determines instruction support since a specific iteretion of Z system
// Other bits determing instruction support in other systems
// Example: 0010 1010 0101:
//  -   0010 xxxx xxxx -> Instruction supported on ESA systems
//  -   xxxx 1010 xxxx -> Instruction supported on 370 systems and is also part of universal instruction set
//  -   xxxx xxxx 0101 -> Instruction supported since Z12 systems up to now
enum class supported_system
{
    NO_Z_SUPPORT = 0,
    SINCE_ZOP = 1,
    SINCE_YOP,
    SINCE_Z9,
    SINCE_Z10,
    SINCE_Z11,
    SINCE_Z12,
    SINCE_Z13,
    SINCE_Z14,
    SINCE_Z15,
    UNI = 1 << 5,
    DOS = 1 << 6,
    _370 = 1 << 7,
    XA = 1 << 8,
    ESA = 1 << 9,
    UNKNOWN = 1 << 10
};

constexpr supported_system operator|(supported_system a, supported_system b)
{
    return static_cast<supported_system>(static_cast<int>(a) | static_cast<int>(b));
}

constexpr supported_system operator&(supported_system a, supported_system b)
{
    return static_cast<supported_system>(static_cast<int>(a) & static_cast<int>(b));
}

constexpr supported_system operator&(supported_system a, size_t b)
{
    return static_cast<supported_system>(static_cast<int>(a) & static_cast<int>(b));
}

constexpr bool operator<=(supported_system a, system_architecture b)
{
    return static_cast<int>(a) <= static_cast<int>(b);
}
} // namespace

std::string_view instruction::mach_format_to_string(mach_format f)
{
    switch (f)
    {
        case mach_format::E:
            return "E";
        case mach_format::I:
            return "I";
        case mach_format::IE:
            return "IE";
        case mach_format::MII:
            return "MII";
        case mach_format::RI_a:
            return "RI-a";
        case mach_format::RI_b:
            return "RI-b";
        case mach_format::RI_c:
            return "RI-c";
        case mach_format::RIE_a:
            return "RIE-a";
        case mach_format::RIE_b:
            return "RIE-b";
        case mach_format::RIE_c:
            return "RIE-c";
        case mach_format::RIE_d:
            return "RIE-d";
        case mach_format::RIE_e:
            return "RIE-e";
        case mach_format::RIE_f:
            return "RIE-f";
        case mach_format::RIE_g:
            return "RIE-g";
        case mach_format::RIL_a:
            return "RIL-a";
        case mach_format::RIL_b:
            return "RIL-b";
        case mach_format::RIL_c:
            return "RIL-c";
        case mach_format::RIS:
            return "RIS";
        case mach_format::RR:
            return "RR";
        case mach_format::RRD:
            return "RRD";
        case mach_format::RRE:
            return "RRE";
        case mach_format::RRF_a:
            return "RRF-a";
        case mach_format::RRF_b:
            return "RRF-b";
        case mach_format::RRF_c:
            return "RRF-c";
        case mach_format::RRF_d:
            return "RRF-d";
        case mach_format::RRF_e:
            return "RRF-e";
        case mach_format::RRS:
            return "RRS";
        case mach_format::RS_a:
            return "RS-a";
        case mach_format::RS_b:
            return "RS-b";
        case mach_format::RSI:
            return "RSI";
        case mach_format::RSL_a:
            return "RSL-a";
        case mach_format::RSL_b:
            return "RSL-b";
        case mach_format::RSY_a:
            return "RSY-a";
        case mach_format::RSY_b:
            return "RSY-b";
        case mach_format::RX_a:
            return "RX-a";
        case mach_format::RX_b:
            return "RX-b";
        case mach_format::RXE:
            return "RXE";
        case mach_format::RXF:
            return "RXF";
        case mach_format::RXY_a:
            return "RXY-a";
        case mach_format::RXY_b:
            return "RXY-b";
        case mach_format::S:
            return "S";
        case mach_format::SI:
            return "SI";
        case mach_format::SIL:
            return "SIL";
        case mach_format::SIY:
            return "SIY";
        case mach_format::SMI:
            return "SMI";
        case mach_format::SS_a:
            return "SS-a";
        case mach_format::SS_b:
            return "SS-b";
        case mach_format::SS_c:
            return "SS-c";
        case mach_format::SS_d:
            return "SS-d";
        case mach_format::SS_e:
            return "SS-e";
        case mach_format::SS_f:
            return "SS-f";
        case mach_format::SSE:
            return "SSE";
        case mach_format::SSF:
            return "SSF";
        case mach_format::VRI_a:
            return "VRI-a";
        case mach_format::VRI_b:
            return "VRI-b";
        case mach_format::VRI_c:
            return "VRI-c";
        case mach_format::VRI_d:
            return "VRI-d";
        case mach_format::VRI_e:
            return "VRI-e";
        case mach_format::VRI_f:
            return "VRI-f";
        case mach_format::VRI_g:
            return "VRI-g";
        case mach_format::VRI_h:
            return "VRI-h";
        case mach_format::VRI_i:
            return "VRI-i";
        case mach_format::VRR_a:
            return "VRR-a";
        case mach_format::VRR_b:
            return "VRR-b";
        case mach_format::VRR_c:
            return "VRR-c";
        case mach_format::VRR_d:
            return "VRR-d";
        case mach_format::VRR_e:
            return "VRR-e";
        case mach_format::VRR_f:
            return "VRR-f";
        case mach_format::VRR_g:
            return "VRR-g";
        case mach_format::VRR_h:
            return "VRR-h";
        case mach_format::VRR_i:
            return "VRR-i";
        case mach_format::VRS_a:
            return "VRS-a";
        case mach_format::VRS_b:
            return "VRS-b";
        case mach_format::VRS_c:
            return "VRS-c";
        case mach_format::VRS_d:
            return "VRS-d";
        case mach_format::VSI:
            return "VSI";
        case mach_format::VRV:
            return "VRV";
        case mach_format::VRX:
            return "VRX";
        default:
            assert(false);
            return "";
    }
}

constexpr ca_instruction ca_instructions[] = {
    { "ACTR", false },
    { "AEJECT", true },
    { "AGO", false },
    { "AIF", false },
    { "ANOP", true },
    { "AREAD", false },
    { "ASPACE", false },
    { "GBLA", false },
    { "GBLB", false },
    { "GBLC", false },
    { "LCLA", false },
    { "LCLB", false },
    { "LCLC", false },
    { "MACRO", true },
    { "MEND", true },
    { "MEXIT", true },
    { "MHELP", false },
    { "SETA", false },
    { "SETB", false },
    { "SETC", false },
};
#if __cpp_lib_ranges
static_assert(std::ranges::is_sorted(ca_instructions, {}, &ca_instruction::name));

const ca_instruction* instruction::find_ca_instructions(std::string_view name)
{
    auto it = std::ranges::lower_bound(ca_instructions, name, {}, &ca_instruction::name);

    if (it == std::ranges::end(ca_instructions) || it->name() != name)
        return nullptr;
    return &*it;
}
#else
static_assert(std::is_sorted(std::begin(ca_instructions), std::end(ca_instructions), [](const auto& l, const auto& r) {
    return l.name() < r.name();
}));

const ca_instruction* instruction::find_ca_instructions(std::string_view name)
{
    auto it = std::lower_bound(
        std::begin(ca_instructions), std::end(ca_instructions), name, [](const auto& l, const auto& r) {
            return l.name() < r;
        });

    if (it == std::end(ca_instructions) || it->name() != name)
        return nullptr;
    return &*it;
}
#endif

const ca_instruction& instruction::get_ca_instructions(std::string_view name)
{
    auto result = find_ca_instructions(name);
    assert(result);
    return *result;
}

std::span<const ca_instruction> instruction::all_ca_instructions() const { return ca_instructions; }

constexpr assembler_instruction assembler_instructions[] = {
    { "*PROCESS", 1, -1, false, "" }, // TO DO
    { "ACONTROL", 1, -1, false, "<selection>+" },
    { "ADATA", 5, 5, false, "value1,value2,value3,value4,character_string" },
    { "AINSERT", 2, 2, false, "'record',BACK|FRONT" },
    { "ALIAS", 1, 1, false, "alias_string" },
    { "AMODE", 1, 1, false, "amode_option" },
    { "CATTR", 1, -1, false, "attribute+" },
    { "CCW", 4, 4, true, "command_code,data_address,flags,data_count" },
    { "CCW0", 4, 4, true, "command_code,data_address,flags,data_count" },
    { "CCW1", 4, 4, true, "command_code,data_address,flags,data_count" },
    { "CEJECT", 0, 1, true, "?number_of_lines" },
    { "CNOP", 2, 2, true, "byte,boundary" },
    { "COM", 0, 0, false, "" },
    { "COPY", 1, 1, false, "member" },
    { "CSECT", 0, 0, false, "" },
    { "CXD", 0, 0, false, "" },
    { "DC", 1, -1, true, "<operand>+" },
    { "DROP", 0, -1, true, "?<<base_register|label>+>", true },
    { "DS", 1, -1, true, "<operand>+" },
    { "DSECT", 0, 0, false, "" },
    { "DXD", 1, -1, true, "<operand>+" },
    { "EJECT", 0, 0, false, "" },
    { "END", 0, 2, true, "?expression,?language" },
    { "ENTRY", 1, -1, true, "entry_point+" },
    { "EQU",
        1,
        5,
        true,
        "value,?<length_attribute_value>,?<type_attribute_value>,?<program_type_value>,?<assembler_type_value>" },
    { "EXITCTL", 2, 5, false, "exit_type,control_value+" },
    { "EXTRN", 1, -1, false, "<external_symbol>+|PART(<external_symbol>+" },
    { "ICTL", 1, 3, false, "begin,?<end>,?<continue>" },
    { "ISEQ", 0, 2, false, "?<left,right>" },
    { "LOCTR", 0, 0, false, "" },
    { "LTORG", 0, 0, false, "" },
    { "MNOTE", 1, 2, false, "?<<severity|*|>,>message" },
    { "OPSYN", 0, 1, false, "?operation_code_2" },
    { "ORG", 0, 3, true, "expression?<,boundary?<,offset>>" },
    { "POP", 1, 4, false, "<PRINT|USING|ACONTROL>+,?NOPRINT" },
    { "PRINT", 1, -1, false, "operand+" },
    { "PUNCH", 1, 1, false, "string" },
    { "PUSH", 1, 4, false, "<PRINT|USING|ACONTROL>+,?NOPRINT" },
    { "REPRO", 0, 0, false, "" },
    { "RMODE", 1, 1, false, "rmode_option" },
    { "RSECT", 0, 0, false, "" },
    { "SPACE", 0, 1, true, "?number_of_lines" },
    { "START", 0, 1, true, "?expression" },
    { "TITLE", 1, 1, false, "title_string" },
    { "USING", 2, 17, true, "operand+", true },
    { "WXTRN", 1, -1, false, "<external_symbol>+|PART(<external_symbol>+" },
    { "XATTR", 1, -1, false, "attribute+" },

};
#ifdef __cpp_lib_ranges
static_assert(std::ranges::is_sorted(assembler_instructions, {}, &assembler_instruction::name));

const assembler_instruction* instruction::find_assembler_instructions(std::string_view instr)
{
    auto it = std::ranges::lower_bound(assembler_instructions, instr, {}, &assembler_instruction::name);
    if (it == std::ranges::end(assembler_instructions) || it->name() != instr)
        return nullptr;
    return &*it;
}
#else
static_assert(std::is_sorted(std::begin(assembler_instructions),
    std::end(assembler_instructions),
    [](const auto& l, const auto& r) { return l.name() < r.name(); }));

const assembler_instruction* instruction::find_assembler_instructions(std::string_view instr) const
{
    auto it = std::lower_bound(
        std::begin(assembler_instructions), std::end(assembler_instructions), instr, [](const auto& l, const auto& r) {
            return l.name() < r;
        });
    if (it == std::end(assembler_instructions) || it->name() != instr)
        return nullptr;
    return &*it;
}
#endif

const assembler_instruction& instruction::get_assembler_instructions(std::string_view instr) const
{
    auto result = find_assembler_instructions(instr);
    assert(result);
    return *result;
}

std::span<const assembler_instruction> instruction::all_assembler_instructions() const
{
    return assembler_instructions;
}

bool hlasm_plugin::parser_library::context::machine_instruction::check_nth_operand(
    size_t place, const checking::machine_operand* operand)
{
    diagnostic_op diag;
    const range stmt_range = range();
    if (operand->check(diag, m_operands[place], m_name.to_string_view(), stmt_range))
        return true;
    return false;
}

bool hlasm_plugin::parser_library::context::machine_instruction::check(std::string_view name_of_instruction,
    const std::vector<const checking::machine_operand*> to_check,
    const range& stmt_range,
    const diagnostic_collector& add_diagnostic) const
{
    // check size of operands
    int diff = (int)m_operand_len - (int)to_check.size();
    if (diff > m_optional_op_count || diff < 0)
    {
        add_diagnostic(diagnostic_op::error_optional_number_of_operands(
            name_of_instruction, m_optional_op_count, (int)m_operand_len, stmt_range));
        return false;
    }
    bool error = false;
    for (size_t i = 0; i < to_check.size(); i++)
    {
        assert(to_check[i] != nullptr);
        diagnostic_op diag;
        if (!(*to_check[i]).check(diag, m_operands[i], name_of_instruction, stmt_range))
        {
            add_diagnostic(diag);
            error = true;
        }
    };
    return (!error);
}

template<mach_format F, const machine_operand_format&... Ops>
class instruction_format_definition_factory
{
    static constexpr std::array<machine_operand_format, sizeof...(Ops)> format = { Ops... };

public:
    static constexpr instruction_format_definition def() { return { { format.data(), sizeof...(Ops) }, F }; }
};
template<mach_format F>
class instruction_format_definition_factory<F>
{
public:
    static constexpr instruction_format_definition def() { return { {}, F }; }
};

// clang-format off
constexpr auto E_0 = instruction_format_definition_factory<mach_format::E>::def();
constexpr auto I_1 = instruction_format_definition_factory<mach_format::I, imm_8_U>::def();
constexpr auto IE_2 = instruction_format_definition_factory<mach_format::IE, imm_4_U, imm_4_U>::def();
constexpr auto MII_3 = instruction_format_definition_factory<mach_format::MII, mask_4_U, rel_addr_imm_12_S, rel_addr_imm_24_S>::def();
constexpr auto RI_a_2_s = instruction_format_definition_factory<mach_format::RI_a, reg_4_U, imm_16_S>::def();
constexpr auto RI_a_2_u = instruction_format_definition_factory<mach_format::RI_a, reg_4_U, imm_16_U>::def();
constexpr auto RI_b_2 = instruction_format_definition_factory<mach_format::RI_b, reg_4_U, rel_addr_imm_16_S>::def();
constexpr auto RI_c_2 = instruction_format_definition_factory<mach_format::RI_c, mask_4_U, rel_addr_imm_16_S>::def();
constexpr auto RIE_a_3 = instruction_format_definition_factory<mach_format::RIE_a, reg_4_U, imm_16_S, mask_4_U>::def();
constexpr auto RIE_b_4 = instruction_format_definition_factory<mach_format::RIE_b, reg_4_U, reg_4_U, mask_4_U, rel_addr_imm_16_S>::def();
constexpr auto RIE_c_4 = instruction_format_definition_factory<mach_format::RIE_c, reg_4_U, imm_8_S, mask_4_U, rel_addr_imm_16_S>::def();
constexpr auto RIE_d_3 = instruction_format_definition_factory<mach_format::RIE_d, reg_4_U, reg_4_U, imm_16_S>::def();
constexpr auto RIE_e_3 = instruction_format_definition_factory<mach_format::RIE_e, reg_4_U, reg_4_U, rel_addr_imm_16_S>::def();
constexpr auto RIE_f_5 = instruction_format_definition_factory<mach_format::RIE_f, reg_4_U, reg_4_U, imm_8_S, imm_8_S, imm_8_S_opt>::def();
constexpr auto RIE_g_3 = instruction_format_definition_factory<mach_format::RIE_g, reg_4_U, imm_16_S, mask_4_U>::def();
constexpr auto RIL_a_2 = instruction_format_definition_factory<mach_format::RIL_a, reg_4_U, imm_32_S>::def();
constexpr auto RIL_b_2 = instruction_format_definition_factory<mach_format::RIL_b, reg_4_U, rel_addr_imm_32_S>::def();
constexpr auto RIL_c_2 = instruction_format_definition_factory<mach_format::RIL_c, mask_4_U, rel_addr_imm_32_S>::def();
constexpr auto RIS_4 = instruction_format_definition_factory<mach_format::RIS, reg_4_U, imm_8_S, mask_4_U, db_12_4_U>::def();
constexpr auto RR_1 = instruction_format_definition_factory<mach_format::RR, reg_4_U>::def();
constexpr auto RR_2_m = instruction_format_definition_factory<mach_format::RR, mask_4_U, reg_4_U>::def();
constexpr auto RR_2 = instruction_format_definition_factory<mach_format::RR, reg_4_U, reg_4_U>::def();
constexpr auto RRD_3 = instruction_format_definition_factory<mach_format::RRD, reg_4_U, reg_4_U, reg_4_U>::def();
constexpr auto RRE_0 = instruction_format_definition_factory<mach_format::RRE>::def();
constexpr auto RRE_1 = instruction_format_definition_factory<mach_format::RRE, reg_4_U>::def();
constexpr auto RRE_2 = instruction_format_definition_factory<mach_format::RRE, reg_4_U, reg_4_U>::def();
constexpr auto RRF_a_3 = instruction_format_definition_factory<mach_format::RRF_a, reg_4_U, reg_4_U, reg_4_U>::def();
constexpr auto RRF_a_4 = instruction_format_definition_factory<mach_format::RRF_a, reg_4_U, reg_4_U, reg_4_U, mask_4_U>::def();
constexpr auto RRF_a_4_opt = instruction_format_definition_factory<mach_format::RRF_a, reg_4_U, reg_4_U, reg_4_U_opt, mask_4_U_opt>::def();
constexpr auto RRF_b_3 = instruction_format_definition_factory<mach_format::RRF_b, reg_4_U, reg_4_U, reg_4_U>::def();
constexpr auto RRF_b_4 = instruction_format_definition_factory<mach_format::RRF_b, reg_4_U, reg_4_U, reg_4_U, mask_4_U>::def();
constexpr auto RRF_b_4_opt = instruction_format_definition_factory<mach_format::RRF_b, reg_4_U, reg_4_U, reg_4_U, mask_4_U_opt>::def();
constexpr auto RRF_c_3 = instruction_format_definition_factory<mach_format::RRF_c, reg_4_U, reg_4_U, mask_4_U>::def();
constexpr auto RRF_c_3_opt = instruction_format_definition_factory<mach_format::RRF_c, reg_4_U, reg_4_U, mask_4_U_opt>::def();
constexpr auto RRF_d_3 = instruction_format_definition_factory<mach_format::RRF_d, reg_4_U, reg_4_U, mask_4_U>::def();
constexpr auto RRF_e_3 = instruction_format_definition_factory<mach_format::RRF_e, reg_4_U, mask_4_U, reg_4_U>::def();
constexpr auto RRF_e_4 = instruction_format_definition_factory<mach_format::RRF_e, reg_4_U, mask_4_U, reg_4_U, mask_4_U>::def();
constexpr auto RRS_4 = instruction_format_definition_factory<mach_format::RRS, reg_4_U, reg_4_U, mask_4_U, db_12_4_U>::def();
constexpr auto RS_a_2 = instruction_format_definition_factory<mach_format::RS_a, reg_4_U, db_12_4_U>::def();
constexpr auto RS_a_3 = instruction_format_definition_factory<mach_format::RS_a, reg_4_U, reg_4_U, db_12_4_U>::def();
constexpr auto RS_b_3 = instruction_format_definition_factory<mach_format::RS_b, reg_4_U, mask_4_U, db_12_4_U>::def();
constexpr auto RSI_3 = instruction_format_definition_factory<mach_format::RSI, reg_4_U, reg_4_U, rel_addr_imm_16_S>::def();
constexpr auto RSL_a_1 = instruction_format_definition_factory<mach_format::RSL_a, db_12_4x4L_U>::def();
constexpr auto RSL_b_3 = instruction_format_definition_factory<mach_format::RSL_b, reg_4_U, db_12_8x4L_U, mask_4_U>::def();
constexpr auto RSY_a_3 = instruction_format_definition_factory<mach_format::RSY_a, reg_4_U, reg_4_U, db_20_4_S>::def();
constexpr auto RSY_b_3_su = instruction_format_definition_factory<mach_format::RSY_b, reg_4_U, db_20_4_S, mask_4_U>::def();
constexpr auto RSY_b_3_us = instruction_format_definition_factory<mach_format::RSY_b, reg_4_U, mask_4_U, db_20_4_S>::def();
constexpr auto RSY_b_3_ux = instruction_format_definition_factory<mach_format::RSY_b, reg_4_U, mask_4_U, dxb_20_4x4_S>::def();
constexpr auto RX_a_2_ux = instruction_format_definition_factory<mach_format::RX_a, reg_4_U, dxb_12_4x4_U>::def();
constexpr auto RX_a_2 = instruction_format_definition_factory<mach_format::RX_a, reg_4_U, reg_4_U>::def();
constexpr auto RX_b_2 = instruction_format_definition_factory<mach_format::RX_b, mask_4_U, dxb_12_4x4_U>::def();
constexpr auto RXE_2 = instruction_format_definition_factory<mach_format::RXE, reg_4_U, dxb_12_4x4_U>::def();
constexpr auto RXE_3_xm = instruction_format_definition_factory<mach_format::RXE, reg_4_U, dxb_12_4x4_U, mask_4_U>::def();
constexpr auto RXF_3_x = instruction_format_definition_factory<mach_format::RXF, reg_4_U, reg_4_U, dxb_12_4x4_U>::def();
constexpr auto RXY_a_2 = instruction_format_definition_factory<mach_format::RXY_a, reg_4_U, dxb_20_4x4_S>::def();
constexpr auto RXY_b_2 = instruction_format_definition_factory<mach_format::RXY_b, mask_4_U, dxb_20_4x4_S>::def();
constexpr auto S_0 = instruction_format_definition_factory<mach_format::S>::def();
constexpr auto S_1_u = instruction_format_definition_factory<mach_format::S, db_12_4_U>::def();
constexpr auto S_1_s = instruction_format_definition_factory<mach_format::S, db_20_4_S>::def();
constexpr auto SI_1 = instruction_format_definition_factory<mach_format::SI, db_12_4_U>::def();
constexpr auto SI_2_s = instruction_format_definition_factory<mach_format::SI, db_12_4_U, imm_8_S>::def();
constexpr auto SI_2_u = instruction_format_definition_factory<mach_format::SI, db_12_4_U, imm_8_U>::def();
constexpr auto SIL_2_s = instruction_format_definition_factory<mach_format::SIL, db_12_4_U, imm_16_S>::def();
constexpr auto SIL_2_u = instruction_format_definition_factory<mach_format::SIL, db_12_4_U, imm_16_U>::def();
constexpr auto SIY_2_ss = instruction_format_definition_factory<mach_format::SIY, db_20_4_S, imm_8_S>::def();
constexpr auto SIY_2_su = instruction_format_definition_factory<mach_format::SIY, db_20_4_S, imm_8_U>::def();
constexpr auto SMI_3 = instruction_format_definition_factory<mach_format::SMI, mask_4_U, rel_addr_imm_16_S, db_12_4_U>::def();
constexpr auto SS_a_2_u = instruction_format_definition_factory<mach_format::SS_a, db_12_8x4L_U, db_12_4_U>::def();
constexpr auto SS_a_2_s = instruction_format_definition_factory<mach_format::SS_a, db_12_8x4L_U, db_20_4_S>::def();
constexpr auto SS_b_2 = instruction_format_definition_factory<mach_format::SS_b, db_12_4x4L_U, db_12_4x4L_U>::def();
constexpr auto SS_c_3 = instruction_format_definition_factory<mach_format::SS_c, db_12_4x4L_U, db_12_4_U, imm_4_U>::def();
constexpr auto SS_d_3 = instruction_format_definition_factory<mach_format::SS_d, drb_12_4x4_U, db_12_4_U, reg_4_U>::def();
constexpr auto SS_e_4_br = instruction_format_definition_factory<mach_format::SS_e, reg_4_U, db_12_4_U, reg_4_U, db_12_4_U>::def();
constexpr auto SS_e_4_rb = instruction_format_definition_factory<mach_format::SS_e, reg_4_U, reg_4_U, db_12_4_U, db_12_4_U>::def();
constexpr auto SS_f_2 = instruction_format_definition_factory<mach_format::SS_f, db_12_4_U, db_12_8x4L_U>::def();
constexpr auto SSE_2 = instruction_format_definition_factory<mach_format::SSE, db_12_4_U, db_12_4_U>::def();
constexpr auto SSF_3_dr = instruction_format_definition_factory<mach_format::SSF, db_12_4_U, db_12_4_U, reg_4_U>::def();
constexpr auto SSF_3_rd = instruction_format_definition_factory<mach_format::SSF, reg_4_U, db_12_4_U, db_12_4_U>::def();
constexpr auto VRI_a_2 = instruction_format_definition_factory<mach_format::VRI_a, vec_reg_5_U, imm_16_U>::def();
constexpr auto VRI_a_3 = instruction_format_definition_factory<mach_format::VRI_a, vec_reg_5_U, imm_16_S, mask_4_U>::def();
constexpr auto VRI_b_4 = instruction_format_definition_factory<mach_format::VRI_b, vec_reg_5_U, imm_8_U, imm_8_U, mask_4_U>::def();
constexpr auto VRI_c_4 = instruction_format_definition_factory<mach_format::VRI_c, vec_reg_5_U, vec_reg_5_U, imm_16_U, mask_4_U>::def();
constexpr auto VRI_d_4 = instruction_format_definition_factory<mach_format::VRI_d, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, imm_8_U>::def();
constexpr auto VRI_d_5 = instruction_format_definition_factory<mach_format::VRI_d, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, imm_8_U, mask_4_U>::def();
constexpr auto VRI_e_5 = instruction_format_definition_factory<mach_format::VRI_e, vec_reg_5_U, vec_reg_5_U, imm_12_S, mask_4_U, mask_4_U>::def();
constexpr auto VRI_f_5 = instruction_format_definition_factory<mach_format::VRI_f, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, imm_8_U, mask_4_U>::def();
constexpr auto VRI_g_5_s = instruction_format_definition_factory<mach_format::VRI_g, vec_reg_5_U, vec_reg_5_U, imm_8_U, imm_8_S, mask_4_U>::def();
constexpr auto VRI_g_5_u = instruction_format_definition_factory<mach_format::VRI_g, vec_reg_5_U, vec_reg_5_U, imm_8_U, imm_8_U, mask_4_U>::def();
constexpr auto VRI_h_3 = instruction_format_definition_factory<mach_format::VRI_h, vec_reg_5_U, imm_16_S, imm_4_U>::def();
constexpr auto VRI_i_4 = instruction_format_definition_factory<mach_format::VRI_i, vec_reg_5_U, reg_4_U, imm_8_S, mask_4_U>::def();
constexpr auto VRR_a_2 = instruction_format_definition_factory<mach_format::VRR_a, vec_reg_5_U, vec_reg_5_U>::def();
constexpr auto VRR_a_3 = instruction_format_definition_factory<mach_format::VRR_a, vec_reg_5_U, vec_reg_5_U, mask_4_U>::def();
constexpr auto VRR_a_4 = instruction_format_definition_factory<mach_format::VRR_a, vec_reg_5_U, vec_reg_5_U, mask_4_U, mask_4_U>::def();
constexpr auto VRR_a_4_opt = instruction_format_definition_factory<mach_format::VRR_a, vec_reg_5_U, vec_reg_5_U, mask_4_U, mask_4_U_opt>::def();
constexpr auto VRR_a_5 = instruction_format_definition_factory<mach_format::VRR_a, vec_reg_5_U, vec_reg_5_U, mask_4_U, mask_4_U, mask_4_U>::def();
constexpr auto VRR_b_5 = instruction_format_definition_factory<mach_format::VRR_b, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, mask_4_U, mask_4_U>::def();
constexpr auto VRR_b_5_opt = instruction_format_definition_factory<mach_format::VRR_b, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, mask_4_U, mask_4_U_opt>::def();
constexpr auto VRR_c_3 = instruction_format_definition_factory<mach_format::VRR_c, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U>::def();
constexpr auto VRR_c_4 = instruction_format_definition_factory<mach_format::VRR_c, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, mask_4_U>::def();
constexpr auto VRR_c_5 = instruction_format_definition_factory<mach_format::VRR_c, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, mask_4_U, mask_4_U>::def();
constexpr auto VRR_c_6 = instruction_format_definition_factory<mach_format::VRR_c, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, mask_4_U, mask_4_U, mask_4_U>::def();
constexpr auto VRR_d_5 = instruction_format_definition_factory<mach_format::VRR_d, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, mask_4_U>::def();
constexpr auto VRR_d_6 = instruction_format_definition_factory<mach_format::VRR_d, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, mask_4_U, mask_4_U>::def();
constexpr auto VRR_d_6_opt = instruction_format_definition_factory<mach_format::VRR_d, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, mask_4_U, mask_4_U_opt>::def();
constexpr auto VRR_e_4 = instruction_format_definition_factory<mach_format::VRR_e, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U>::def();
constexpr auto VRR_e_6 = instruction_format_definition_factory<mach_format::VRR_e, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, vec_reg_5_U, mask_4_U, mask_4_U>::def();
constexpr auto VRR_f_3 = instruction_format_definition_factory<mach_format::VRR_f, vec_reg_5_U, reg_4_U, reg_4_U>::def();
constexpr auto VRR_g_1 = instruction_format_definition_factory<mach_format::VRR_g, vec_reg_5_U>::def();
constexpr auto VRR_h_3 = instruction_format_definition_factory<mach_format::VRR_h, vec_reg_5_U, vec_reg_5_U, mask_4_U>::def();
constexpr auto VRR_i_3 = instruction_format_definition_factory<mach_format::VRR_i, reg_4_U, vec_reg_5_U, mask_4_U>::def();
constexpr auto VRS_a_4 = instruction_format_definition_factory<mach_format::VRS_a, vec_reg_5_U, vec_reg_5_U, db_12_4_U, mask_4_U>::def();
constexpr auto VRS_a_4_opt = instruction_format_definition_factory<mach_format::VRS_a, vec_reg_5_U, vec_reg_5_U, db_12_4_U, mask_4_U_opt>::def();
constexpr auto VRS_b_3 = instruction_format_definition_factory<mach_format::VRS_b, vec_reg_5_U, reg_4_U, db_12_4_U>::def();
constexpr auto VRS_b_4 = instruction_format_definition_factory<mach_format::VRS_b, vec_reg_5_U, reg_4_U, db_12_4_U, mask_4_U>::def();
constexpr auto VRS_c_4 = instruction_format_definition_factory<mach_format::VRS_c, reg_4_U, vec_reg_5_U, db_12_4_U, mask_4_U>::def();
constexpr auto VRS_d_3 = instruction_format_definition_factory<mach_format::VRS_d, vec_reg_5_U, reg_4_U, db_12_4_U>::def();
constexpr auto VRV_3 = instruction_format_definition_factory<mach_format::VRV, vec_reg_5_U, dvb_12_5x4_U, mask_4_U>::def();
constexpr auto VRX_3 = instruction_format_definition_factory<mach_format::VRX, vec_reg_5_U, dxb_12_4x4_U, mask_4_U>::def();
constexpr auto VRX_3_opt = instruction_format_definition_factory<mach_format::VRX, vec_reg_5_U, dxb_12_4x4_U, mask_4_U_opt>::def();
constexpr auto VSI_3 = instruction_format_definition_factory<mach_format::VSI, vec_reg_5_U, db_12_4_U, imm_8_U>::def();

constexpr std::pair<machine_instruction, supported_system> machine_instructions[] = {
    { { "A", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "AD", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "ADB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "ADBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "ADDFRR", RRE_2 }, { supported_system::UNKNOWN } }, 
    { { "ADR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "ADTR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "ADTRA", RRF_a_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "AE", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "AEB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "AEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "AER", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "AFI", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "AG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "AGF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "AGFI", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "AGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "AGH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "AGHI", RI_a_2_s }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "AGHIK", RIE_d_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "AGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "AGRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "AGSI", SIY_2_ss }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "AH", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "AHHHR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "AHHLR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "AHI", RI_a_2_s }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "AHIK", RIE_d_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "AHY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "AIH", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "AL", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "ALC", RXY_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "ALCG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "ALCGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "ALCR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "ALFI", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "ALG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "ALGF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "ALGFI", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "ALGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "ALGHSIK", RIE_d_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "ALGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "ALGRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "ALGSI", SIY_2_ss }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "ALHHHR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "ALHHLR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "ALHSIK", RIE_d_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "ALR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "ALRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "ALSI", SIY_2_ss }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "ALSIH", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "ALSIHN", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "ALY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "AP", SS_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "AR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "ARK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "ASI", SIY_2_ss }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "AU", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "AUR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "AW", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "AWR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "AXBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "AXR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "AXTR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "AXTRA", RRF_a_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "AY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "BAKR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BAL", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BALR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BAS", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "BASR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "BASSM", RX_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "BC", RX_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BCR", RR_2_m }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BCT", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BCTG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BCTGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BCTR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BIC", RXY_b_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BPP", SMI_3 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "BPRP", MII_3 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "BRAS", RI_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRASL", RIL_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRC", RI_c_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRCL", RIL_c_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRCT", RI_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRCTG", RI_b_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRCTH", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "BRXH", RSI_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRXHG", RIE_e_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRXLE", RSI_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRXLG", RIE_e_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BSA", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BSG", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BSM", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "BXH", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BXHG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BXLE", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BXLEG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "C", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CD", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CDB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CDBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CDFBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CDFBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CDFR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CDFTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CDGBR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CDGBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CDGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CDGTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CDGTRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CDLFBR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CDLFTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CDLGBR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CDLGTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CDPT", RSL_b_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "CDR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CDS", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CDSG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CDSTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CDSY", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CDTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CDUTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CDZT", RSL_b_3 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CE", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CEB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CEDTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CEFBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CEFBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CEFR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CEGBR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CEGBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CEGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CELFBR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CELGBR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CER", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CEXTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CFC", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "CFDBR", RRF_e_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CFDBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CFDR", RRF_e_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CFDTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CFEBR", RRF_e_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CFEBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CFER", RRF_e_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CFI", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CFXBR", RRF_e_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CFXBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CFXR", RRF_e_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CFXTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CGDBR", RRF_e_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CGDBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CGDR", RRF_e_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CGDTR", RRF_e_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CGDTRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CGEBR", RRF_e_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CGEBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CGER", RRF_e_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CGF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CGFI", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CGFRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGHI", RI_a_2_s }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CGHRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGHSI", SIL_2_s }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIB", RIS_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIJ", RIE_c_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIT", RIE_a_3 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CGRB", RRS_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRJ", RIE_b_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRT", RRF_c_3 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGXBR", RRF_e_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CGXBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CGXR", RRF_e_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CGXTR", RRF_e_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CGXTRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CH", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CHF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CHHR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CHHSI", SIL_2_s }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CHI", RI_a_2_s }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CHLR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CHRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CHSI", SIL_2_s }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CHY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CIB", RIS_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIH", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CIJ", RIE_c_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIT", RIE_a_3 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CKSM", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CL", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CLC", SS_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CLCL", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CLCLE", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CLCLU", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CLFDBR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLFDTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLFEBR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLFHSI", SIL_2_u }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLFI", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CLFIT", RIE_a_3 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLFXBR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLFXTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CLGDBR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLGDTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLGEBR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLGF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CLGFI", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CLGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CLGFRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGHRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGHSI", SIL_2_u }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIB", RIS_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIJ", RIE_c_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIT", RIE_a_3 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CLGRB", RRS_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRJ", RIE_b_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRT", RRF_c_3 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGT", RSY_b_3_ux }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CLGXBR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLGXTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLHF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLHHR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLHHSI", SIL_2_u }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLHLR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLHRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLI", SI_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CLIB", RIS_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLIH", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CLIJ", RIE_c_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLIY", SIY_2_su }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CLM", RS_b_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CLMH", RSY_b_3_us }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CLMY", RSY_b_3_us }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CLR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CLRB", RRS_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRCH", S_1_u }, { supported_system::UNI | supported_system::_370 } }, 
    { { "CLRIO", S_1_u }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "CLRJ", RIE_b_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRT", RRF_c_3 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLST", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CLT", RSY_b_3_ux }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CLY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CMPSC", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CONCS", S_1_u }, { supported_system::UNI | supported_system::_370 } }, 
    { { "CP", SS_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CPDT", RSL_b_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "CPSDR", RRF_b_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CPXT", RSL_b_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "CPYA", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CRB", RRS_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRDTE", RRF_b_4_opt }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CRJ", RIE_b_4 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRT", RRF_c_3 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CS", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CSCH", S_0 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "CSDTR", RRF_d_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CSG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CSP", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CSPG", RRE_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CSST", SSF_3_dr }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CSXTR", RRF_d_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CSY", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CU12", RRF_c_3_opt }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CU14", RRF_c_3_opt }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CU21", RRF_c_3_opt }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CU24", RRF_c_3_opt }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CU41", RRE_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CU42", RRE_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CUDTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CUSE", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CUTFU", RRF_c_3_opt }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CUUTF", RRF_c_3_opt }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CUXTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CVB", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CVBG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CVBY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CVD", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CVDG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CVDY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CXBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CXFBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CXFBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CXFR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CXFTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CXGBR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CXGBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CXGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "CXGTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CXGTRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CXLFBR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CXLFTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CXLGBR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CXLGTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "CXPT", RSL_b_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "CXR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "CXSTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CXTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CXUTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "CXZT", RSL_b_3 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "CZDT", RSL_b_3 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CZXT", RSL_b_3 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "D", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "DD", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "DDB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "DDBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "DDR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "DDTR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "DDTRA", RRF_a_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "DE", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "DEB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "DEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "DER", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "DFLTCC", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "DIDBR", RRF_b_4 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "DIEBR", RRF_b_4 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "DISCS", S_1_u }, { supported_system::UNI | supported_system::_370 } }, 
    { { "DL", RXY_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "DLG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "DLGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "DLR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "DP", SS_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "DR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "DSG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "DSGF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "DSGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "DSGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "DXBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "DXR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "DXTR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "DXTRA", RRF_a_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "EAR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "ECAG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "ECCTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "ECPGA", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "ECTG", SSF_3_dr }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "ED", SS_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "EDMK", SS_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "EEDTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "EEXTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "EFPC", RRE_1 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "EPAIR", RRE_1 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "EPAR", RRE_1 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "EPCTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "EPSW", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "EREG", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "EREGG", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "ESAIR", RRE_1 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "ESAR", RRE_1 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "ESDTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "ESEA", RRE_1 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "ESTA", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "ESXTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "ETND", RRE_1 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "EX", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "EXRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "FIDBR", RRF_e_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "FIDBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "FIDR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "FIDTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "FIEBR", RRF_e_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "FIEBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "FIER", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "FIXBR", RRF_e_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "FIXBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "FIXR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "FIXTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "FLOGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "HDR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "HDV", S_1_u }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "HER", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "HIO", S_1_u }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "HSCH", S_0 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "IAC", RRE_1 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "IC", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "ICM", RS_b_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "ICMH", RSY_b_3_us }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "ICMY", RSY_b_3_us }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "ICY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "IDTE", RRF_b_4_opt }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "IEDTR", RRF_b_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "IEXTR", RRF_b_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "IIHF", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "IIHH", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "IIHL", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "IILF", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "IILH", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "IILL", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "IPK", S_0 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "IPM", RRE_1 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "IPTE", RRF_a_4_opt }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "IRBM", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "ISK", RR_2 }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "ISKE", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "IVSK", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "KDB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "KDBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "KDSA", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "KDTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "KEB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "KEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "KIMD", RRE_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "KLMD", RRE_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "KM", RRE_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "KMA", RRF_b_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "KMAC", RRE_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "KMC", RRE_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "KMCTR", RRF_b_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "KMF", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "KMO", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "KXBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "KXTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "L", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LA", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LAA", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LAAG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LAAL", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LAALG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LAE", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LAEY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LAM", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LAMY", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "LAN", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LANG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LAO", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LAOG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LARL", RIL_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LASP", SSE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "LAT", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "LAX", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LAXG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LAY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "LB", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "LBH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LBR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LCBB", RXE_3_xm }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LCCTL", S_1_u }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LCDBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LCDFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LCDR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LCEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LCER", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LCGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LCGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LCR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LCTL", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LCTLG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LCXBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LCXR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LD", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LDE", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LDEB", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LDEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LDER", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LDETR", RRF_d_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LDGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LDR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LDXBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LDXBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LDXR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LDXTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LDY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "LE", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LEDBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LEDBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LEDR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LEDTR", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LER", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LEXBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LEXBRA", RRF_e_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LEXR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LEY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "LFAS", S_1_u }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LFH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LFHAT", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "LFPC", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LGAT", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "LGB", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "LGBR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LGDR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LGF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LGFI", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LGFRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LGG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "LGH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LGHI", RI_a_2_s }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LGHR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LGHRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LGRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LGSC", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "LH", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LHH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LHI", RI_a_2_s }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LHR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LHRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LHY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "LLC", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LLCH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LLCR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LLGC", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LLGCR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LLGF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LLGFAT", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "LLGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LLGFRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LLGFSG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "LLGH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LLGHR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LLGHRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LLGT", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LLGTAT", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "LLGTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LLH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LLHH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LLHR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LLHRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LLIHF", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LLIHH", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LLIHL", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LLILF", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LLILH", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LLILL", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LLZRGF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LM", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LMD", SS_e_4_rb }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LMG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LMH", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LMY", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "LNDBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LNDFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LNDR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LNEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LNER", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LNGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LNGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LNR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LNXBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LNXR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LOC", RSY_b_3_su }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCFH", RSY_b_3_su }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHR", RRF_c_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCG", RSY_b_3_su }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCGHI", RIE_g_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCGR", RRF_c_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCHHI", RIE_g_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHI", RIE_g_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCR", RRF_c_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LPCTL", S_1_u }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LPD", SSF_3_rd }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LPDBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LPDFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LPDG", SSF_3_rd }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LPDR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LPEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LPER", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LPGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LPGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LPP", S_1_u }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LPQ", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LPR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LPSW", SI_1 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LPSWE", S_1_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LPTEA", RRF_b_4 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LPXBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LPXR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LRA", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LRAG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LRAY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "LRDR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LRER", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LRV", RXY_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LRVG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LRVGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LRVH", RXY_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LRVR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LSCTL", S_1_u }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LT", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LTDBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LTDR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LTDTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LTEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LTER", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LTG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LTGF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "LTGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LTGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LTR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "LTXBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LTXR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LTXTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LURA", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LURAG", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "LXD", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LXDB", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LXDBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LXDR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LXDTR", RRF_d_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "LXE", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LXEB", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LXEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LXER", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LXR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "LZDR", RRE_1 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LZER", RRE_1 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LZRF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LZRG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LZXR", RRE_1 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "M", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MAD", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "MADB", RXF_3_x }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MADBR", RRD_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MADR", RRD_3 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "MAE", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "MAEB", RXF_3_x }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MAEBR", RRD_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MAER", RRD_3 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "MAY", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MAYH", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MAYHR", RRD_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MAYL", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MAYLR", RRD_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MAYR", RRD_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MC", SI_2_s }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MD", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MDB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MDBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MDE", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MDEB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MDEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MDER", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MDR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MDTR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MDTRA", RRF_a_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "ME", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MEE", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MEEB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MEEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MEER", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MER", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MFY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "MG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "MGH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "MGHI", RI_a_2_s }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "MGRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "MH", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MHI", RI_a_2_s }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MHY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "ML", RXY_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MLG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "MLGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "MLR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MP", SS_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MS", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MSC", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "MSCH", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "MSD", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "MSDB", RXF_3_x }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MSDBR", RRD_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MSDR", RRD_3 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "MSE", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "MSEB", RXF_3_x }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MSEBR", RRD_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MSER", RRD_3 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "MSFI", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "MSG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "MSGC", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "MSGF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "MSGFI", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "MSGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "MSGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "MSGRKC", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "MSR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MSRKC", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "MSTA", RRE_1 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MSY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "MVC", SS_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MVCDK", SSE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MVCIN", SS_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MVCK", SS_d_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "MVCL", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MVCLE", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MVCLU", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "MVCOS", SSF_3_dr }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MVCP", SS_d_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "MVCRL", SSE_2 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "MVCS", SS_d_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "MVCSK", SSE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MVGHI", SIL_2_s }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "MVHHI", SIL_2_s }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "MVHI", SIL_2_s }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "MVI", SI_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MVIY", SIY_2_su }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "MVN", SS_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MVO", SS_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MVPG", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MVST", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MVZ", SS_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MXBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MXD", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MXDB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MXDBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "MXDR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MXR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "MXTR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MXTRA", RRF_a_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "MY", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MYH", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MYHR", RRD_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MYL", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MYLR", RRD_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "MYR", RRD_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "N", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "NC", SS_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "NCGRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "NCRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "NG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "NGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "NGRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "NI", SI_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "NIAI", IE_2 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "NIHF", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "NIHH", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "NIHL", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "NILF", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "NILH", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "NILL", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "NIY", SIY_2_su }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "NNGRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "NNRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "NOGRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "NORK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "NR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "NRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "NTSTG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "NXGRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "NXRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "NY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "O", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "OC", SS_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "OCGRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "OCRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "OG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "OGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "OGRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "OI", SI_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "OIHF", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "OIHH", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "OIHL", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "OILF", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "OILH", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "OILL", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "OIY", SIY_2_su }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "OR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "ORK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "OY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "PACK", SS_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "PALB", RRE_0 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "PC", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "PCC", RRE_0 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "PCKMO", RRE_0 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "PFD", RXY_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "PFDRL", RIL_c_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "PFMF", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "PFPO", E_0 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "PGIN", RRE_2 }, { supported_system::UNKNOWN } }, 
    { { "PGOUT", RRE_2 }, { supported_system::UNKNOWN } }, 
    { { "PKA", SS_f_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "PKU", SS_f_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "PLO", SS_e_4_br }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "POPCNT", RRF_c_3_opt }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "PPA", RRF_c_3 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "PPNO", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "PR", E_0 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "PRNO", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "PT", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "PTF", RRE_1 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "PTFF", E_0 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "PTI", RRE_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "PTLB", S_0 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "QADTR", RRF_b_4 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "QAXTR", RRF_b_4 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "QCTRI", S_1_u }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "QSI", S_1_u }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "RCHP", S_0 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "RISBG", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "RISBGN", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "RISBGNZ", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "RISBGZ", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "RISBHG", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "RISBHGZ", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "RISBLG", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "RISBLGZ", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "RLL", RSY_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "RLLG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "RNSBG", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "RNSBGT", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "ROSBG", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "ROSBGT", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "RP", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "RRB", S_1_u }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "RRBE", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "RRBM", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "RRDTR", RRF_b_4 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "RRXTR", RRF_b_4 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "RSCH", S_0 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "RXSBG", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "RXSBGT", RIE_f_5 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "S", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SAC", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "SACF", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SAL", S_0 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "SAM24", E_0 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SAM31", E_0 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SAM64", E_0 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SAR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SCCTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "SCHM", S_0 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "SCK", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SCKC", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SCKPF", E_0 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SD", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SDB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SDBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SDR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SDTR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "SDTRA", RRF_a_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SE", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SEB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SELFHR", RRF_a_4 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELGR", RRF_a_4 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELR", RRF_a_4 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SER", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SFASR", RRE_1 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "SFPC", RRE_1 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SGF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SGH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "SGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SGRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SH", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SHHHR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SHHLR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SHY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "SIE", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "SIGP", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "SIO", S_1_u }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "SIOF", S_1_u }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "SL", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SLA", RS_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SLAG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SLAK", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SLB", RXY_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SLBG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SLBGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SLBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SLDA", RS_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SLDL", RS_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SLDT", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "SLFI", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "SLG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SLGF", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SLGFI", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "SLGFR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SLGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SLGRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SLHHHR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SLHHLR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SLL", RS_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SLLG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SLLK", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SLR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SLRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SLXT", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "SLY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "SORTL", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SP", SS_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SPCTR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "SPKA", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SPM", RR_1 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SPT", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SPX", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "SQD", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SQDB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SQDBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SQDR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "SQE", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SQEB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SQEBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SQER", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "SQXBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SQXR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SRA", RS_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SRAG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SRAK", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SRDA", RS_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SRDL", RS_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SRDT", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "SRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SRL", RS_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SRLG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "SRLK", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SRNM", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SRNMB", S_1_u }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SRNMT", S_1_u }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "SRP", SS_c_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SRST", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SRSTU", RRE_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "SRXT", RXF_3_x }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "SSAIR", RRE_1 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "SSAR", RRE_1 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "SSCH", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "SSK", RR_2 }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "SSKE", RRF_c_3_opt }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "SSM", SI_1 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "ST", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STAM", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "STAMY", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "STAP", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "STC", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STCH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STCK", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STCKC", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STCKE", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "STCKF", S_1_u }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "STCM", RS_b_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STCMH", RSY_b_3_us }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "STCMY", RSY_b_3_us }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "STCPS", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "STCRW", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "STCTG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "STCTL", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STCY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "STD", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STDY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "STE", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STEY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "STFH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STFL", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "STFLE", S_1_s }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "STFPC", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "STG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "STGRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "STGSC", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "STH", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STHH", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STHRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "STHY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "STIDC", S_1_u }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "STIDP", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STM", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STMG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "STMH", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "STMY", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "STNSM", SI_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STOC", RSY_b_3_su }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOCFH", RSY_b_3_su }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "STOCG", RSY_b_3_su }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOSM", SI_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STPQ", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "STPT", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "STPX", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "STRAG", SSE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "STRL", RIL_b_2 }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "STRV", RXY_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "STRVG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "STRVH", RXY_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "STSCH", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "STSI", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "STURA", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "STURG", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "STY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "SU", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SUR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SVC", I_1 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SW", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SWR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SXBR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "SXR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "SXTR", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "SXTRA", RRF_a_4 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "TABORT", S_1_u }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "TAM", E_0 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TAR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TB", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "TBDR", RRF_e_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TBEDR", RRF_e_3 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TBEGIN", SIL_2_s }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "TBEGINC", SIL_2_s }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "TCDB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TCEB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TCH", S_1_u }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "TCXB", RXE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TDCDT", RXE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "TDCET", RXE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "TDCXT", RXE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "TDGDT", RXE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "TDGET", RXE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "TDGXT", RXE_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "TEND", S_0 }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "THDER", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "THDR", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TIO", S_1_u }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "TM", SI_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "TMH", RI_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TMHH", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "TMHL", RI_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "TML", RI_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TMLH", RI_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TMLL", RI_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TMY", SIY_2_su }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "TP", RSL_a_1 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "TPEI", RRE_2 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "TPI", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "TPROT", SSE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZOP } }, 
    { { "TR", SS_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "TRACE", RS_a_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "TRACG", RSY_a_3 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "TRAP2", E_0 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TRAP4", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TRE", RRE_2 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "TROO", RRF_c_3_opt }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "TROT", RRF_c_3_opt }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "TRT", SS_a_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "TRTE", RRF_c_3_opt }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "TRTO", RRF_c_3_opt }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "TRTR", SS_a_2_u }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "TRTRE", RRF_c_3_opt }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "TRTT", RRF_c_3_opt }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "TS", SI_1 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "TSCH", S_1_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "UNPK", SS_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "UNPKA", SS_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "UNPKU", SS_a_2_u }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "UPT", E_0 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZOP } }, 
    { { "VA", VRR_c_4 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VAC", VRR_d_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VACC", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VACCC", VRR_d_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VACD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VACE", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VACRS", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VACSV", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VAD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VADS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VAE", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VAES", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VAP", VRI_f_5 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VAS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VAVG", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VAVGL", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VBPERM", VRR_c_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VC", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCDS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCE", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCEQ", VRR_b_5 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VCES", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCFPL", VRR_a_5 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VCFPS", VRR_a_5 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VCH", VRR_b_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHL", VRR_b_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCKSM", VRR_c_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCLFP", VRR_a_5 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VCLGD", VRR_a_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCLZ", VRR_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCOVM", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCP", VRR_h_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VCS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCSFP", VRR_a_5 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VCTZ", VRR_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCVB", VRR_i_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VCVBG", VRR_i_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VCVD", VRI_i_4 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VCVDG", VRI_i_4 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VCVM", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCZVM", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VDD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VDDS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VDE", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VDES", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VDP", VRI_f_5 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VEC", VRR_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VECL", VRR_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERIM", VRI_d_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERLL", VRS_a_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERLLV", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESL", VRS_a_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESLV", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRA", VRS_a_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRAV", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRL", VRS_a_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRLV", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFA", VRR_c_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFAE", VRR_b_5_opt }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFCE", VRR_c_6 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFCH", VRR_c_6 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFCHE", VRR_c_6 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFD", VRR_c_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFEE", VRR_b_5_opt }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFENE", VRR_b_5_opt }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFI", VRR_a_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFLL", VRR_a_4 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFLR", VRR_a_5 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFM", VRR_c_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFMA", VRR_e_6 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFMAX", VRR_c_6 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFMIN", VRR_c_6 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFMS", VRR_e_6 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFNMA", VRR_e_6 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFNMS", VRR_e_6 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFPSO", VRR_a_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFS", VRR_c_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFSQ", VRR_a_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFTCI", VRI_e_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGBM", VRI_a_2 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGEF", VRV_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGEG", VRV_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGFM", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGFMA", VRR_d_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGM", VRI_b_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VISTR", VRR_a_4_opt }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VL", VRX_3_opt }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VLBB", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLBIX", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLBR", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLBRREP", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLC", VRR_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLCVM", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLEB", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLEBRG", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLEBRH", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLEF", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLEG", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLEH", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLEIB", VRI_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLEIF", VRI_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLEIG", VRI_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLEIH", VRI_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLELD", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLELE", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLER", VRX_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z15 } }, 
    { { "VLGV", VRS_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLH", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLI", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLID", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLINT", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLIP", VRI_h_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VLL", VRS_b_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLLEBRZ", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLLEZ", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLM", VRS_a_4_opt }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VLMD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLP", VRR_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLR", VRR_a_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VLREP", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLRL", VSI_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VLRLR", VRS_d_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VLVCA", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLVCU", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLVG", VRS_b_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLVGP", VRR_f_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLVM", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLY", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLYD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VM", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMAD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMADS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMAE", VRR_d_5 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VMAES", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMAH", VRR_d_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMAL", VRR_d_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALE", VRR_d_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALH", VRR_d_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALO", VRR_d_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMAO", VRR_d_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMCD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMCE", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMDS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VME", VRR_c_4 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VMES", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMH", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VML", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLE", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLH", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLO", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMN", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMNL", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMNSD", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMNSE", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMO", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMP", VRI_f_5 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VMRH", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMRL", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMRRS", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMRSV", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMSD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMSDS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMSE", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMSES", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMSL", VRR_d_6 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VMSP", VRI_f_5 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VMX", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMXAD", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMXAE", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMXL", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMXSE", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VN", VRR_c_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VNC", VRR_c_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VNN", VRR_c_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VNO", VRR_c_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VNS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VNVM", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VNX", VRR_c_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VO", VRR_c_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VOC", VRR_c_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VOS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VOVM", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VPDI", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPERM", VRR_e_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPK", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKLS", VRR_b_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKS", VRR_b_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKZ", VSI_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VPOPCT", VRR_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPSOP", VRI_g_5_u }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VRCL", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VREP", VRI_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VREPI", VRI_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VRP", VRI_f_5 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VRRS", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VRSV", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VRSVC", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VS", VRR_c_4 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VSBCBI", VRR_d_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSBI", VRR_d_5 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSCBI", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSCEF", VRV_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSCEG", VRV_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSDP", VRI_f_5 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VSDS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSE", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSEG", VRR_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSEL", VRR_e_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSES", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSL", VRR_c_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSLB", VRR_c_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSLD", VRI_d_4 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSLDB", VRI_d_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSLL", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSP", VRI_f_5 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VSPSD", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSRA", VRR_c_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSRAB", VRR_c_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSRD", VRI_d_4 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSRL", VRR_c_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VSRLB", VRR_c_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSRP", VRI_g_5_s }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VSRRS", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSRSV", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VST", VRX_3_opt }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VSTBR", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTEB", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTEBRF", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTEBRG", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTEBRH", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTEF", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTEG", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTEH", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTER", VRX_3 }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTH", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTI", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTID", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTK", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTKD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTL", VRS_b_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTM", VRS_a_4_opt }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VSTMD", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTRC", VRR_d_6_opt }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTRL", VSI_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VSTRLR", VRS_d_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VSTRS", VRR_d_6_opt }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTVM", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTVP", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSUM", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSUMG", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSUMQ", VRR_c_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSVMM", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VTM", VRR_a_2 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VTP", VRR_g_1 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VTVM", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VUPH", VRR_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPKZ", VSI_3 }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VUPL", VRR_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPLH", VRR_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPLL", VRR_a_3 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VX", VRR_c_3 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VXELD", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VXELE", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VXS", RI_a_2_u }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VXVC", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VXVM", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VXVMM", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VZPSD", RRE_2 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "WFC", VRR_a_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFK", VRR_a_4 }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "X", RX_a_2_ux }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "XC", SS_a_2_s }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "XG", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "XGR", RRE_2 }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "XGRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "XI", SI_2_u }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "XIHF", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "XILF", RIL_a_2 }, { supported_system::UNI | supported_system::SINCE_Z9 } }, 
    { { "XIY", SIY_2_su }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "XR", RR_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "XRK", RRF_a_3 }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "XSCH", S_0 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "XY", RXY_a_2 }, { supported_system::UNI | supported_system::SINCE_YOP } }, 
    { { "ZAP", SS_b_2 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
};
// clang-format on

#ifdef __cpp_lib_ranges
static_assert(std::ranges::is_sorted(machine_instructions, {}, [](const auto& instr) { return instr.first.name(); }));

const machine_instruction* instruction::find_machine_instructions(std::string_view name)
{
    auto it = std::ranges::lower_bound(m_machine_instructions, name, {}, &machine_instruction::name);
    if (it == std::ranges::end(m_machine_instructions) || it->get().name() != name)
        return nullptr;
    return &it->get();
}

constexpr const machine_instruction* find_mi(std::string_view name)
{
    auto it =
        std::ranges::lower_bound(machine_instructions, name, {}, [](const auto& instr) { return instr.first.name(); });

    assert(it != std::ranges::end(machine_instructions) && it->first.name() == name);
    return &it->first;
}
#else
static_assert(std::is_sorted(std::begin(machine_instructions),
    std::end(machine_instructions),
    [](const auto& l, const auto& r) { return l.first.name() < r.first.name(); }));

const machine_instruction* instruction::find_machine_instructions(std::string_view name) const
{
    auto it = std::lower_bound(
        std::begin(m_machine_instructions), std::end(m_machine_instructions), name, [](const auto& l, const auto& r) {
            return l.get().name() < r;
        });
    if (it == std::end(m_machine_instructions) || it->get().name() != name)
        return nullptr;
    return &it->get();
}

constexpr const machine_instruction* find_mi(std::string_view name)
{
    auto it = std::lower_bound(
        std::begin(machine_instructions), std::end(machine_instructions), name, [](const auto& l, const auto& r) {
            return l.first.name() < r;
        });
    assert(it != std::end(machine_instructions) && it->first.name() == name);
    return &it->first;
}
#endif

const machine_instruction& instruction::get_machine_instructions(std::string_view name) const
{
    auto mi = find_machine_instructions(name);
    assert(mi);
    return *mi;
}

std::span<const std::reference_wrapper<const machine_instruction>> instruction::all_machine_instructions() const
{
    return m_machine_instructions;
}

constexpr auto mi_BC = find_mi("BC");
constexpr auto mi_BCR = find_mi("BCR");
constexpr auto mi_BIC = find_mi("BIC");
constexpr auto mi_BRAS = find_mi("BRAS");
constexpr auto mi_BRASL = find_mi("BRASL");
constexpr auto mi_BRC = find_mi("BRC");
constexpr auto mi_BRCL = find_mi("BRCL");
constexpr auto mi_BRCT = find_mi("BRCT");
constexpr auto mi_BRCTG = find_mi("BRCTG");
constexpr auto mi_BRXH = find_mi("BRXH");
constexpr auto mi_BRXHG = find_mi("BRXHG");
constexpr auto mi_BRXLE = find_mi("BRXLE");
constexpr auto mi_BRXLG = find_mi("BRXLG");
constexpr auto mi_CGIB = find_mi("CGIB");
constexpr auto mi_CGIJ = find_mi("CGIJ");
constexpr auto mi_CGIT = find_mi("CGIT");
constexpr auto mi_CGRB = find_mi("CGRB");
constexpr auto mi_CGRJ = find_mi("CGRJ");
constexpr auto mi_CGRT = find_mi("CGRT");
constexpr auto mi_CIB = find_mi("CIB");
constexpr auto mi_CIJ = find_mi("CIJ");
constexpr auto mi_CIT = find_mi("CIT");
constexpr auto mi_CLFIT = find_mi("CLFIT");
constexpr auto mi_CLGIB = find_mi("CLGIB");
constexpr auto mi_CLGIJ = find_mi("CLGIJ");
constexpr auto mi_CLGIT = find_mi("CLGIT");
constexpr auto mi_CLGRB = find_mi("CLGRB");
constexpr auto mi_CLGRJ = find_mi("CLGRJ");
constexpr auto mi_CLGRT = find_mi("CLGRT");
constexpr auto mi_CLGT = find_mi("CLGT");
constexpr auto mi_CLIB = find_mi("CLIB");
constexpr auto mi_CLIJ = find_mi("CLIJ");
constexpr auto mi_CLRB = find_mi("CLRB");
constexpr auto mi_CLRJ = find_mi("CLRJ");
constexpr auto mi_CLRT = find_mi("CLRT");
constexpr auto mi_CLT = find_mi("CLT");
constexpr auto mi_CRB = find_mi("CRB");
constexpr auto mi_CRJ = find_mi("CRJ");
constexpr auto mi_CRT = find_mi("CRT");
constexpr auto mi_LOC = find_mi("LOC");
constexpr auto mi_LOCFH = find_mi("LOCFH");
constexpr auto mi_LOCFHR = find_mi("LOCFHR");
constexpr auto mi_LOCG = find_mi("LOCG");
constexpr auto mi_LOCGHI = find_mi("LOCGHI");
constexpr auto mi_LOCGR = find_mi("LOCGR");
constexpr auto mi_LOCHHI = find_mi("LOCHHI");
constexpr auto mi_LOCHI = find_mi("LOCHI");
constexpr auto mi_LOCR = find_mi("LOCR");
constexpr auto mi_NOGRK = find_mi("NOGRK");
constexpr auto mi_NORK = find_mi("NORK");
constexpr auto mi_RISBHGZ = find_mi("RISBHGZ");
constexpr auto mi_RISBLGZ = find_mi("RISBLGZ");
constexpr auto mi_RNSBG = find_mi("RNSBG");
constexpr auto mi_ROSBG = find_mi("ROSBG");
constexpr auto mi_RXSBG = find_mi("RXSBG");
constexpr auto mi_SELFHR = find_mi("SELFHR");
constexpr auto mi_SELGR = find_mi("SELGR");
constexpr auto mi_SELR = find_mi("SELR");
constexpr auto mi_STOC = find_mi("STOC");
constexpr auto mi_STOCFH = find_mi("STOCFH");
constexpr auto mi_STOCG = find_mi("STOCG");
constexpr auto mi_VA = find_mi("VA");
constexpr auto mi_VAC = find_mi("VAC");
constexpr auto mi_VACC = find_mi("VACC");
constexpr auto mi_VACCC = find_mi("VACCC");
constexpr auto mi_VAVG = find_mi("VAVG");
constexpr auto mi_VAVGL = find_mi("VAVGL");
constexpr auto mi_VCEQ = find_mi("VCEQ");
constexpr auto mi_VCFPL = find_mi("VCFPL");
constexpr auto mi_VCFPS = find_mi("VCFPS");
constexpr auto mi_VCH = find_mi("VCH");
constexpr auto mi_VCHL = find_mi("VCHL");
constexpr auto mi_VCLFP = find_mi("VCLFP");
constexpr auto mi_VCLGD = find_mi("VCLGD");
constexpr auto mi_VCLZ = find_mi("VCLZ");
constexpr auto mi_VCSFP = find_mi("VCSFP");
constexpr auto mi_VEC = find_mi("VEC");
constexpr auto mi_VECL = find_mi("VECL");
constexpr auto mi_VERIM = find_mi("VERIM");
constexpr auto mi_VERLL = find_mi("VERLL");
constexpr auto mi_VERLLV = find_mi("VERLLV");
constexpr auto mi_VESL = find_mi("VESL");
constexpr auto mi_VESLV = find_mi("VESLV");
constexpr auto mi_VESRA = find_mi("VESRA");
constexpr auto mi_VESRAV = find_mi("VESRAV");
constexpr auto mi_VESRL = find_mi("VESRL");
constexpr auto mi_VESRLV = find_mi("VESRLV");
constexpr auto mi_VFA = find_mi("VFA");
constexpr auto mi_VFAE = find_mi("VFAE");
constexpr auto mi_VFCE = find_mi("VFCE");
constexpr auto mi_VFCH = find_mi("VFCH");
constexpr auto mi_VFCHE = find_mi("VFCHE");
constexpr auto mi_VFD = find_mi("VFD");
constexpr auto mi_VFEE = find_mi("VFEE");
constexpr auto mi_VFENE = find_mi("VFENE");
constexpr auto mi_VFI = find_mi("VFI");
constexpr auto mi_VFLL = find_mi("VFLL");
constexpr auto mi_VFLR = find_mi("VFLR");
constexpr auto mi_VFM = find_mi("VFM");
constexpr auto mi_VFMA = find_mi("VFMA");
constexpr auto mi_VFMAX = find_mi("VFMAX");
constexpr auto mi_VFMIN = find_mi("VFMIN");
constexpr auto mi_VFMS = find_mi("VFMS");
constexpr auto mi_VFNMA = find_mi("VFNMA");
constexpr auto mi_VFNMS = find_mi("VFNMS");
constexpr auto mi_VFPSO = find_mi("VFPSO");
constexpr auto mi_VFS = find_mi("VFS");
constexpr auto mi_VFSQ = find_mi("VFSQ");
constexpr auto mi_VFTCI = find_mi("VFTCI");
constexpr auto mi_VGBM = find_mi("VGBM");
constexpr auto mi_VGFM = find_mi("VGFM");
constexpr auto mi_VGFMA = find_mi("VGFMA");
constexpr auto mi_VGM = find_mi("VGM");
constexpr auto mi_VISTR = find_mi("VISTR");
constexpr auto mi_VLBR = find_mi("VLBR");
constexpr auto mi_VLBRREP = find_mi("VLBRREP");
constexpr auto mi_VLC = find_mi("VLC");
constexpr auto mi_VLER = find_mi("VLER");
constexpr auto mi_VLGV = find_mi("VLGV");
constexpr auto mi_VLLEBRZ = find_mi("VLLEBRZ");
constexpr auto mi_VLLEZ = find_mi("VLLEZ");
constexpr auto mi_VLP = find_mi("VLP");
constexpr auto mi_VLREP = find_mi("VLREP");
constexpr auto mi_VLVG = find_mi("VLVG");
constexpr auto mi_VMAE = find_mi("VMAE");
constexpr auto mi_VMAH = find_mi("VMAH");
constexpr auto mi_VMAL = find_mi("VMAL");
constexpr auto mi_VMALE = find_mi("VMALE");
constexpr auto mi_VMALH = find_mi("VMALH");
constexpr auto mi_VMALO = find_mi("VMALO");
constexpr auto mi_VMAO = find_mi("VMAO");
constexpr auto mi_VME = find_mi("VME");
constexpr auto mi_VMH = find_mi("VMH");
constexpr auto mi_VML = find_mi("VML");
constexpr auto mi_VMLE = find_mi("VMLE");
constexpr auto mi_VMLH = find_mi("VMLH");
constexpr auto mi_VMLO = find_mi("VMLO");
constexpr auto mi_VMN = find_mi("VMN");
constexpr auto mi_VMNL = find_mi("VMNL");
constexpr auto mi_VMO = find_mi("VMO");
constexpr auto mi_VMRH = find_mi("VMRH");
constexpr auto mi_VMRL = find_mi("VMRL");
constexpr auto mi_VMSL = find_mi("VMSL");
constexpr auto mi_VMX = find_mi("VMX");
constexpr auto mi_VMXL = find_mi("VMXL");
constexpr auto mi_VNO = find_mi("VNO");
constexpr auto mi_VPK = find_mi("VPK");
constexpr auto mi_VPKLS = find_mi("VPKLS");
constexpr auto mi_VPKS = find_mi("VPKS");
constexpr auto mi_VPOPCT = find_mi("VPOPCT");
constexpr auto mi_VREP = find_mi("VREP");
constexpr auto mi_VREPI = find_mi("VREPI");
constexpr auto mi_VS = find_mi("VS");
constexpr auto mi_VSBCBI = find_mi("VSBCBI");
constexpr auto mi_VSBI = find_mi("VSBI");
constexpr auto mi_VSCBI = find_mi("VSCBI");
constexpr auto mi_VSEG = find_mi("VSEG");
constexpr auto mi_VSTBR = find_mi("VSTBR");
constexpr auto mi_VSTEBRF = find_mi("VSTEBRF");
constexpr auto mi_VSTEBRG = find_mi("VSTEBRG");
constexpr auto mi_VSTER = find_mi("VSTER");
constexpr auto mi_VSTRC = find_mi("VSTRC");
constexpr auto mi_VSTRS = find_mi("VSTRS");
constexpr auto mi_VSUM = find_mi("VSUM");
constexpr auto mi_VSUMG = find_mi("VSUMG");
constexpr auto mi_VSUMQ = find_mi("VSUMQ");
constexpr auto mi_VUPH = find_mi("VUPH");
constexpr auto mi_VUPL = find_mi("VUPL");
constexpr auto mi_VUPLH = find_mi("VUPLH");
constexpr auto mi_VUPLL = find_mi("VUPLL");
constexpr auto mi_WFC = find_mi("WFC");
constexpr auto mi_WFK = find_mi("WFK");

// clang-format off
constexpr std::pair<mnemonic_code, supported_system> mnemonic_codes[] = {
    { { "B", mi_BC, { { 0, 15 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BE", mi_BC, { { 0, 8 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BER", mi_BCR, { { 0, 8 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BH", mi_BC, { { 0, 2 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BHR", mi_BCR, { { 0, 2 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BI", mi_BIC, { { 0, 15 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BIE", mi_BIC, { { 0, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BIH", mi_BIC, { { 0, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BIL", mi_BIC, { { 0, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BIM", mi_BIC, { { 0, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BINE", mi_BIC, { { 0, 7 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BINH", mi_BIC, { { 0, 13 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BINL", mi_BIC, { { 0, 11 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BINM", mi_BIC, { { 0, 11 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BINO", mi_BIC, { { 0, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BINP", mi_BIC, { { 0, 13 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BINZ", mi_BIC, { { 0, 7 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BIO", mi_BIC, { { 0, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BIP", mi_BIC, { { 0, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BIZ", mi_BIC, { { 0, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "BL", mi_BC, { { 0, 4 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BLR", mi_BCR, { { 0, 4 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BM", mi_BC, { { 0, 4 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BMR", mi_BCR, { { 0, 4 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNE", mi_BC, { { 0, 7 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNER", mi_BCR, { { 0, 7 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNH", mi_BC, { { 0, 13 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNHR", mi_BCR, { { 0, 13 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNL", mi_BC, { { 0, 11 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNLR", mi_BCR, { { 0, 11 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNM", mi_BC, { { 0, 11 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNMR", mi_BCR, { { 0, 11 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNO", mi_BC, { { 0, 14 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNOR", mi_BCR, { { 0, 14 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNP", mi_BC, { { 0, 13 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNPR", mi_BCR, { { 0, 13 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNZ", mi_BC, { { 0, 7 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BNZR", mi_BCR, { { 0, 7 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BO", mi_BC, { { 0, 1 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BOR", mi_BCR, { { 0, 1 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BP", mi_BC, { { 0, 2 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BPR", mi_BCR, { { 0, 2 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BR", mi_BCR, { { 0, 15 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BRE", mi_BRC, { { 0, 8 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BREL", mi_BRCL, { { 0, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRH", mi_BRC, { { 0, 2 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRHL", mi_BRCL, { { 0, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRL", mi_BRC, { { 0, 4 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRLL", mi_BRCL, { { 0, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRM", mi_BRC, { { 0, 4 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRML", mi_BRCL, { { 0, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRNE", mi_BRC, { { 0, 7 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRNEL", mi_BRCL, { { 0, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRNH", mi_BRC, { { 0, 13 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRNHL", mi_BRCL, { { 0, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRNL", mi_BRC, { { 0, 11 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRNLL", mi_BRCL, { { 0, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRNM", mi_BRC, { { 0, 11 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRNML", mi_BRCL, { { 0, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRNO", mi_BRC, { { 0, 14 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRNOL", mi_BRCL, { { 0, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRNP", mi_BRC, { { 0, 13 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRNPL", mi_BRCL, { { 0, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRNZ", mi_BRC, { { 0, 7 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRNZL", mi_BRCL, { { 0, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRO", mi_BRC, { { 0, 1 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BROL", mi_BRCL, { { 0, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRP", mi_BRC, { { 0, 2 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRPL", mi_BRCL, { { 0, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRU", mi_BRC, { { 0, 15 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRUL", mi_BRCL, { { 0, 15 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BRZ", mi_BRC, { { 0, 8 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "BRZL", mi_BRCL, { { 0, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "BZ", mi_BC, { { 0, 8 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "BZR", mi_BCR, { { 0, 8 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "CGIBE", mi_CGIB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIBH", mi_CGIB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIBL", mi_CGIB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIBNE", mi_CGIB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIBNH", mi_CGIB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIBNL", mi_CGIB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIJE", mi_CGIJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIJH", mi_CGIJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIJL", mi_CGIJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIJNE", mi_CGIJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIJNH", mi_CGIJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGIJNL", mi_CGIJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGITE", mi_CGIT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGITH", mi_CGIT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGITL", mi_CGIT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGITNE", mi_CGIT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGITNH", mi_CGIT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGITNL", mi_CGIT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRBE", mi_CGRB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRBH", mi_CGRB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRBL", mi_CGRB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRBNE", mi_CGRB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRBNH", mi_CGRB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRBNL", mi_CGRB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRJE", mi_CGRJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRJH", mi_CGRJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRJL", mi_CGRJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRJNE", mi_CGRJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRJNH", mi_CGRJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRJNL", mi_CGRJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRTE", mi_CGRT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRTH", mi_CGRT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRTL", mi_CGRT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRTNE", mi_CGRT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRTNH", mi_CGRT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CGRTNL", mi_CGRT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIBE", mi_CIB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIBH", mi_CIB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIBL", mi_CIB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIBNE", mi_CIB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIBNH", mi_CIB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIBNL", mi_CIB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIJE", mi_CIJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIJH", mi_CIJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIJL", mi_CIJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIJNE", mi_CIJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIJNH", mi_CIJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CIJNL", mi_CIJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CITE", mi_CIT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CITH", mi_CIT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CITL", mi_CIT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CITNE", mi_CIT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CITNH", mi_CIT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CITNL", mi_CIT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLFITE", mi_CLFIT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLFITH", mi_CLFIT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLFITL", mi_CLFIT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLFITNE", mi_CLFIT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLFITNH", mi_CLFIT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLFITNL", mi_CLFIT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIBE", mi_CLGIB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIBH", mi_CLGIB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIBL", mi_CLGIB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIBNE", mi_CLGIB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIBNH", mi_CLGIB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIBNL", mi_CLGIB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIJE", mi_CLGIJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIJH", mi_CLGIJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIJL", mi_CLGIJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIJNE", mi_CLGIJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIJNH", mi_CLGIJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGIJNL", mi_CLGIJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGITE", mi_CLGIT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGITH", mi_CLGIT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGITL", mi_CLGIT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGITNE", mi_CLGIT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGITNH", mi_CLGIT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGITNL", mi_CLGIT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRBE", mi_CLGRB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRBH", mi_CLGRB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRBL", mi_CLGRB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRBNE", mi_CLGRB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRBNH", mi_CLGRB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRBNL", mi_CLGRB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRJE", mi_CLGRJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRJH", mi_CLGRJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRJL", mi_CLGRJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRJNE", mi_CLGRJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRJNH", mi_CLGRJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRJNL", mi_CLGRJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRTE", mi_CLGRT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRTH", mi_CLGRT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRTL", mi_CLGRT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRTNE", mi_CLGRT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRTNH", mi_CLGRT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGRTNL", mi_CLGRT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLGTE", mi_CLGT, { { 1, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CLGTH", mi_CLGT, { { 1, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CLGTL", mi_CLGT, { { 1, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CLGTNE", mi_CLGT, { { 1, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CLGTNH", mi_CLGT, { { 1, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CLGTNL", mi_CLGT, { { 1, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CLIBE", mi_CLIB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLIBH", mi_CLIB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLIBL", mi_CLIB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLIBNE", mi_CLIB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLIBNH", mi_CLIB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLIBNL", mi_CLIB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLIJE", mi_CLIJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLIJH", mi_CLIJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLIJL", mi_CLIJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLIJNE", mi_CLIJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLIJNH", mi_CLIJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLIJNL", mi_CLIJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRBE", mi_CLRB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRBH", mi_CLRB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRBL", mi_CLRB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRBNE", mi_CLRB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRBNH", mi_CLRB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRBNL", mi_CLRB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRJE", mi_CLRJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRJH", mi_CLRJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRJL", mi_CLRJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRJNE", mi_CLRJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRJNH", mi_CLRJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRJNL", mi_CLRJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRTE", mi_CLRT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRTH", mi_CLRT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRTL", mi_CLRT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRTNE", mi_CLRT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRTNH", mi_CLRT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLRTNL", mi_CLRT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CLTE", mi_CLT, { { 1, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CLTH", mi_CLT, { { 1, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CLTL", mi_CLT, { { 1, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CLTNE", mi_CLT, { { 1, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CLTNH", mi_CLT, { { 1, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CLTNL", mi_CLT, { { 1, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z12 } }, 
    { { "CRBE", mi_CRB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRBH", mi_CRB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRBL", mi_CRB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRBNE", mi_CRB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRBNH", mi_CRB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRBNL", mi_CRB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRJE", mi_CRJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRJH", mi_CRJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRJL", mi_CRJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRJNE", mi_CRJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRJNH", mi_CRJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRJNL", mi_CRJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRTE", mi_CRT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRTH", mi_CRT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRTL", mi_CRT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRTNE", mi_CRT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRTNH", mi_CRT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "CRTNL", mi_CRT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z10 } }, 
    { { "J", mi_BRC, { { 0, 15 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JAS", mi_BRAS, {} }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JASL", mi_BRASL, {} }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JC", mi_BRC, {} }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JCT", mi_BRCT, {} }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JCTG", mi_BRCTG, {} }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JE", mi_BRC, { { 0, 8 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JH", mi_BRC, { { 0, 2 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JL", mi_BRC, { { 0, 4 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JLE", mi_BRCL, { { 0, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLH", mi_BRCL, { { 0, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLL", mi_BRCL, { { 0, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLM", mi_BRCL, { { 0, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLNE", mi_BRCL, { { 0, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLNH", mi_BRCL, { { 0, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLNL", mi_BRCL, { { 0, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLNM", mi_BRCL, { { 0, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLNO", mi_BRCL, { { 0, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLNOP", mi_BRCL, { { 0, 0 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JLNP", mi_BRCL, { { 0, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLNZ", mi_BRCL, { { 0, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLO", mi_BRCL, { { 0, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLP", mi_BRCL, { { 0, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLU", mi_BRCL, { { 0, 15 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JLZ", mi_BRCL, { { 0, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JM", mi_BRC, { { 0, 4 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JNE", mi_BRC, { { 0, 7 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JNH", mi_BRC, { { 0, 13 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JNL", mi_BRC, { { 0, 11 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JNM", mi_BRC, { { 0, 11 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JNO", mi_BRC, { { 0, 14 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JNOP", mi_BRC, { { 0, 0 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JNP", mi_BRC, { { 0, 13 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JNZ", mi_BRC, { { 0, 7 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JO", mi_BRC, { { 0, 1 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JP", mi_BRC, { { 0, 2 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JXH", mi_BRXH, {} }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JXHG", mi_BRXHG, {} }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JXLE", mi_BRXLE, {} }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "JXLEG", mi_BRXLG, {} }, { supported_system::UNI | supported_system::SINCE_ZOP } }, 
    { { "JZ", mi_BRC, { { 0, 8 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZOP } }, 
    { { "LDRV", mi_VLLEBRZ, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "LERV", mi_VLLEBRZ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "LHHR", mi_RISBHGZ, { { 2, 0 }, { 3, 31 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LHLR", mi_RISBHGZ, { { 2, 0 }, { 3, 31 }, { 4, 32 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LLCHHR", mi_RISBHGZ, { { 2, 24 }, { 3, 31 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LLCHLR", mi_RISBHGZ, { { 2, 24 }, { 3, 31 }, { 4, 32 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LLCLHR", mi_RISBLGZ, { { 2, 24 }, { 3, 31 }, { 4, 32 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LLHFR", mi_RISBLGZ, { { 2, 0 }, { 3, 31 }, { 4, 32 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LLHHHR", mi_RISBHGZ, { { 2, 16 }, { 3, 31 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LLHHLR", mi_RISBHGZ, { { 2, 16 }, { 3, 31 }, { 4, 32 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LLHLHR", mi_RISBLGZ, { { 2, 16 }, { 3, 31 }, { 4, 32 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCE", mi_LOC, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCFHE", mi_LOCFH, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHH", mi_LOCFH, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHL", mi_LOCFH, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHNE", mi_LOCFH, { { 2, 7 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHNH", mi_LOCFH, { { 2, 13 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHNL", mi_LOCFH, { { 2, 11 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHNO", mi_LOCFH, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHO", mi_LOCFH, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHRE", mi_LOCFHR, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHRH", mi_LOCFHR, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHRL", mi_LOCFHR, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHRNE", mi_LOCFHR, { { 2, 7 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHRNH", mi_LOCFHR, { { 2, 13 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHRNL", mi_LOCFHR, { { 2, 11 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHRNO", mi_LOCFHR, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCFHRO", mi_LOCFHR, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCGE", mi_LOCG, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCGH", mi_LOCG, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCGHIE", mi_LOCGHI, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCGHIH", mi_LOCGHI, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCGHIL", mi_LOCGHI, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCGHINE", mi_LOCGHI, { { 2, 7 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCGHINH", mi_LOCGHI, { { 2, 13 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCGHINL", mi_LOCGHI, { { 2, 11 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCGHINO", mi_LOCGHI, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCGHIO", mi_LOCGHI, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCGL", mi_LOCG, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCGNE", mi_LOCG, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCGNH", mi_LOCG, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCGNL", mi_LOCG, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCGNO", mi_LOCG, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCGO", mi_LOCG, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCGRE", mi_LOCGR, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCGRH", mi_LOCGR, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCGRL", mi_LOCGR, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCGRNE", mi_LOCGR, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCGRNH", mi_LOCGR, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCGRNL", mi_LOCGR, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCGRNO", mi_LOCGR, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCGRO", mi_LOCGR, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCH", mi_LOC, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCHHIE", mi_LOCHHI, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHHIH", mi_LOCHHI, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHHIL", mi_LOCHHI, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHHINE", mi_LOCHHI, { { 2, 7 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHHINH", mi_LOCHHI, { { 2, 13 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHHINL", mi_LOCHHI, { { 2, 11 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHHINO", mi_LOCHHI, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHHIO", mi_LOCHHI, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHIE", mi_LOCHI, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHIH", mi_LOCHI, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHIL", mi_LOCHI, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHINE", mi_LOCHI, { { 2, 7 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHINH", mi_LOCHI, { { 2, 13 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHINL", mi_LOCHI, { { 2, 11 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHINO", mi_LOCHI, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCHIO", mi_LOCHI, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCL", mi_LOC, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCNE", mi_LOC, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCNH", mi_LOC, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCNL", mi_LOC, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCNO", mi_LOC, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCO", mi_LOC, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCRE", mi_LOCR, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCRH", mi_LOCR, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCRL", mi_LOCR, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCRNE", mi_LOCR, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCRNH", mi_LOCR, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCRNL", mi_LOCR, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "LOCRNO", mi_LOCR, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "LOCRO", mi_LOCR, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "NHHR", mi_RNSBG, { { 2, 0 }, { 3, 31 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "NHLR", mi_RNSBG, { { 2, 0 }, { 3, 31 }, { 4, 32 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "NLHR", mi_RNSBG, { { 2, 32 }, { 3, 63 }, { 4, 32 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "NOP", mi_BC, { { 0, 0 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "NOPR", mi_BCR, { { 0, 0 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZOP } }, 
    { { "NOTGR", mi_NOGRK, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "NOTR", mi_NORK, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "OHHR", mi_ROSBG, { { 2, 0 }, { 3, 31 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "OHLR", mi_ROSBG, { { 2, 0 }, { 3, 31 }, { 4, 32 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "OLHR", mi_ROSBG, { { 2, 32 }, { 3, 63 }, { 4, 32 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "SELFHRE", mi_SELFHR, { { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELFHRH", mi_SELFHR, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELFHRL", mi_SELFHR, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELFHRNE", mi_SELFHR, { { 3, 7 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELFHRNH", mi_SELFHR, { { 3, 13 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELFHRNL", mi_SELFHR, { { 3, 11 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELFHRNO", mi_SELFHR, { { 3, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELFHRO", mi_SELFHR, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELGRE", mi_SELGR, { { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELGRH", mi_SELGR, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELGRL", mi_SELGR, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELGRNE", mi_SELGR, { { 3, 7 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELGRNH", mi_SELGR, { { 3, 13 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELGRNL", mi_SELGR, { { 3, 11 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELGRNO", mi_SELGR, { { 3, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELGRO", mi_SELGR, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELRE", mi_SELR, { { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELRH", mi_SELR, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELRL", mi_SELR, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELRNE", mi_SELR, { { 3, 7 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELRNH", mi_SELR, { { 3, 13 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELRNL", mi_SELR, { { 3, 11 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELRNO", mi_SELR, { { 3, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "SELRO", mi_SELR, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "STDRV", mi_VSTEBRG, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "STERV", mi_VSTEBRF, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "STOCE", mi_STOC, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOCFHE", mi_STOCFH, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "STOCFHH", mi_STOCFH, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "STOCFHL", mi_STOCFH, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "STOCFHNE", mi_STOCFH, { { 2, 7 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "STOCFHNH", mi_STOCFH, { { 2, 13 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "STOCFHNL", mi_STOCFH, { { 2, 11 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "STOCFHNO", mi_STOCFH, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "STOCFHO", mi_STOCFH, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "STOCGE", mi_STOCG, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOCGH", mi_STOCG, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOCGL", mi_STOCG, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOCGNE", mi_STOCG, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOCGNH", mi_STOCG, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOCGNL", mi_STOCG, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOCGNO", mi_STOCG, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "STOCGO", mi_STOCG, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "STOCH", mi_STOC, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOCL", mi_STOC, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOCNE", mi_STOC, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOCNH", mi_STOC, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOCNL", mi_STOC, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "STOCNO", mi_STOC, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "STOCO", mi_STOC, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VAB", mi_VA, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VACCB", mi_VACC, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VACCCQ", mi_VACCC, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VACCF", mi_VACC, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VACCG", mi_VACC, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VACCH", mi_VACC, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VACCQ", mi_VACC, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VACQ", mi_VAC, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VAF", mi_VA, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VAG", mi_VA, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VAH", mi_VA, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VAQ", mi_VA, { { 3, 4 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VAVGB", mi_VAVG, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VAVGF", mi_VAVG, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VAVGG", mi_VAVG, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VAVGH", mi_VAVG, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VAVGLB", mi_VAVGL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VAVGLF", mi_VAVGL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VAVGLG", mi_VAVGL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VAVGLH", mi_VAVGL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCDG", mi_VCFPS, {} }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCDGB", mi_VCFPS, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCDLG", mi_VCFPL, {} }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCDLGB", mi_VCFPL, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCEFB", mi_VCFPS, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VCELFB", mi_VCFPL, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VCEQB", mi_VCEQ, { { 3, 0 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCEQBS", mi_VCEQ, { { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCEQF", mi_VCEQ, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCEQFS", mi_VCEQ, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCEQG", mi_VCEQ, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCEQGS", mi_VCEQ, { { 3, 3 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCEQH", mi_VCEQ, { { 3, 1 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCEQHS", mi_VCEQ, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCFEB", mi_VCSFP, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VCGD", mi_VCSFP, {} }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCGDB", mi_VCSFP, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHB", mi_VCH, { { 3, 0 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHBS", mi_VCH, { { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHF", mi_VCH, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHFS", mi_VCH, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHG", mi_VCH, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHGS", mi_VCH, { { 3, 3 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHH", mi_VCH, { { 3, 1 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHHS", mi_VCH, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHLB", mi_VCHL, { { 3, 0 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHLBS", mi_VCHL, { { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHLF", mi_VCHL, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHLFS", mi_VCHL, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHLG", mi_VCHL, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHLGS", mi_VCHL, { { 3, 3 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHLH", mi_VCHL, { { 3, 1 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCHLHS", mi_VCHL, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCLFEB", mi_VCLFP, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VCLGDB", mi_VCLGD, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCLZB", mi_VCLZ, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCLZF", mi_VCLZ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCLZG", mi_VCLZ, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VCLZH", mi_VCLZ, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VECB", mi_VEC, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VECF", mi_VEC, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VECG", mi_VEC, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VECH", mi_VEC, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VECLB", mi_VECL, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VECLF", mi_VECL, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VECLG", mi_VECL, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VECLH", mi_VECL, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERIMB", mi_VERIM, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERIMF", mi_VERIM, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERIMG", mi_VERIM, { { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERIMH", mi_VERIM, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERLLB", mi_VERLL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERLLF", mi_VERLL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERLLG", mi_VERLL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERLLH", mi_VERLL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERLLVB", mi_VERLLV, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERLLVF", mi_VERLLV, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERLLVG", mi_VERLLV, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VERLLVH", mi_VERLLV, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESLB", mi_VESL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESLF", mi_VESL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESLG", mi_VESL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESLH", mi_VESL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESLVB", mi_VESLV, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESLVF", mi_VESLV, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESLVG", mi_VESLV, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESLVH", mi_VESLV, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRAB", mi_VESRA, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRAF", mi_VESRA, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRAG", mi_VESRA, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRAH", mi_VESRA, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRAVB", mi_VESRAV, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRAVF", mi_VESRAV, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRAVG", mi_VESRAV, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRAVH", mi_VESRAV, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRLB", mi_VESRL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRLF", mi_VESRL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRLG", mi_VESRL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRLH", mi_VESRL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRLVB", mi_VESRLV, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRLVF", mi_VESRLV, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRLVG", mi_VESRLV, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VESRLVH", mi_VESRLV, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFADB", mi_VFA, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFAEB", mi_VFAE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFAEBS", mi_VFAE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFAEF", mi_VFAE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFAEFS", mi_VFAE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFAEH", mi_VFAE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFAEHS", mi_VFAE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFAEZB", mi_VFAE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFAEZBS", mi_VFAE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFAEZF", mi_VFAE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFAEZFS", mi_VFAE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFAEZH", mi_VFAE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFAEZHS", mi_VFAE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFASB", mi_VFA, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFCEDB", mi_VFCE, { { 3, 3 }, { 4, 0 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFCEDBS", mi_VFCE, { { 3, 3 }, { 4, 0 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFCESB", mi_VFCE, { { 3, 2 }, { 4, 0 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFCESBS", mi_VFCE, { { 3, 2 }, { 4, 0 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFCHDB", mi_VFCH, { { 3, 3 }, { 4, 0 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFCHDBS", mi_VFCH, { { 3, 3 }, { 4, 0 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFCHEDB", mi_VFCHE, { { 3, 3 }, { 4, 0 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFCHEDBS", mi_VFCHE, { { 3, 3 }, { 4, 0 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFCHESB", mi_VFCHE, { { 3, 2 }, { 4, 0 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFCHESBS", mi_VFCHE, { { 3, 2 }, { 4, 0 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFCHSB", mi_VFCH, { { 3, 2 }, { 4, 0 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFCHSBS", mi_VFCH, { { 3, 2 }, { 4, 0 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFDDB", mi_VFD, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFDSB", mi_VFD, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFEEB", mi_VFEE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFEEBS", mi_VFEE, { { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFEEF", mi_VFEE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFEEFS", mi_VFEE, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFEEGS", mi_VFEE, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNKNOWN } }, 
    { { "VFEEH", mi_VFEE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFEEZB", mi_VFEE, { { 3, 0 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFEEZBS", mi_VFEE, { { 3, 0 }, { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFEEZF", mi_VFEE, { { 3, 2 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFEEZFS", mi_VFEE, { { 3, 2 }, { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFEEZH", mi_VFEE, { { 3, 1 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFEEZHS", mi_VFEE, { { 3, 1 }, { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFENEB", mi_VFENE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFENEBS", mi_VFENE, { { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFENEF", mi_VFENE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFENEFS", mi_VFENE, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFENEH", mi_VFENE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFENEHS", mi_VFENE, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFENEZB", mi_VFENE, { { 3, 0 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFENEZBS", mi_VFENE, { { 3, 0 }, { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFENEZF", mi_VFENE, { { 3, 2 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFENEZFS", mi_VFENE, { { 3, 2 }, { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFENEZH", mi_VFENE, { { 3, 1 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFENEZHS", mi_VFENE, { { 3, 1 }, { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFIDB", mi_VFI, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFISB", mi_VFI, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFKEDB", mi_VFCE, { { 3, 3 }, { 4, 4 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFKEDBS", mi_VFCE, { { 3, 3 }, { 4, 4 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFKESB", mi_VFCE, { { 3, 2 }, { 4, 4 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFKESBS", mi_VFCE, { { 3, 2 }, { 4, 4 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFKHDB", mi_VFCH, { { 3, 3 }, { 4, 4 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFKHDBS", mi_VFCH, { { 3, 3 }, { 4, 4 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFKHEDB", mi_VFCHE, { { 3, 3 }, { 4, 4 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFKHEDBS", mi_VFCHE, { { 3, 3 }, { 4, 4 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFKHESB", mi_VFCHE, { { 3, 2 }, { 4, 4 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFKHESBS", mi_VFCHE, { { 3, 2 }, { 4, 4 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFKHSB", mi_VFCH, { { 3, 2 }, { 4, 4 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFKHSBS", mi_VFCH, { { 3, 2 }, { 4, 4 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFLCDB", mi_VFPSO, { { 2, 3 }, { 3, 0 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFLCSB", mi_VFPSO, { { 2, 2 }, { 3, 0 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFLLS", mi_VFLL, { { 2, 2 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFLNDB", mi_VFPSO, { { 2, 3 }, { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFLNSB", mi_VFPSO, { { 2, 2 }, { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFLPDB", mi_VFPSO, { { 2, 3 }, { 3, 0 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFLPSB", mi_VFPSO, { { 2, 2 }, { 3, 0 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFLRD", mi_VFLR, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFMADB", mi_VFMA, { { 4, 0 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFMASB", mi_VFMA, { { 4, 0 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFMAXDB", mi_VFMAX, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFMAXSB", mi_VFMAX, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFMDB", mi_VFM, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFMINDB", mi_VFMIN, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFMINSB", mi_VFMIN, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFMSB", mi_VFM, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFMSDB", mi_VFMS, { { 4, 0 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFMSSB", mi_VFMS, { { 4, 0 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFNMADB", mi_VFNMA, { { 4, 0 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFNMASB", mi_VFNMA, { { 4, 0 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFNMSDB", mi_VFNMS, { { 4, 0 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFNMSSB", mi_VFNMS, { { 4, 0 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFPSODB", mi_VFPSO, { { 2, 3 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFPSOSB", mi_VFPSO, { { 2, 2 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFSDB", mi_VFS, { { 2, 3 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFSQDB", mi_VFSQ, { { 2, 3 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFSQSB", mi_VFSQ, { { 2, 2 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFSSB", mi_VFS, { { 2, 2 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VFTCIDB", mi_VFTCI, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VFTCISB", mi_VFTCI, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VGFMAB", mi_VGFMA, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGFMAF", mi_VGFMA, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGFMAG", mi_VGFMA, { { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGFMAH", mi_VGFMA, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGFMB", mi_VGFM, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGFMF", mi_VGFM, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGFMG", mi_VGFM, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGFMH", mi_VGFM, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGMB", mi_VGM, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGMF", mi_VGM, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGMG", mi_VGM, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VGMH", mi_VGM, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VISTRB", mi_VISTR, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VISTRBS", mi_VISTR, { { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VISTRF", mi_VISTR, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VISTRFS", mi_VISTR, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VISTRH", mi_VISTR, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VISTRHS", mi_VISTR, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLBRF", mi_VLBR, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLBRG", mi_VLBR, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLBRH", mi_VLBR, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLBRQ", mi_VLBR, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLBRREPF", mi_VLBRREP, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLBRREPG", mi_VLBRREP, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLBRREPH", mi_VLBRREP, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLCB", mi_VLC, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLCF", mi_VLC, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLCG", mi_VLC, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLCH", mi_VLC, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLDE", mi_VFLL, {} }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLDEB", mi_VFLL, { { 2, 2 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLED", mi_VFLR, {} }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLEDB", mi_VFLR, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLERF", mi_VLER, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLERG", mi_VLER, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLERH", mi_VLER, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLGVB", mi_VLGV, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLGVF", mi_VLGV, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLGVG", mi_VLGV, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLGVH", mi_VLGV, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLLEBRZE", mi_VLLEBRZ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLLEBRZF", mi_VLLEBRZ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLLEBRZG", mi_VLLEBRZ, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLLEBRZH", mi_VLLEBRZ, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VLLEZB", mi_VLLEZ, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLLEZF", mi_VLLEZ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLLEZG", mi_VLLEZ, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLLEZH", mi_VLLEZ, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLLEZLF", mi_VLLEZ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VLPB", mi_VLP, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLPF", mi_VLP, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLPG", mi_VLP, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLPH", mi_VLP, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLREPB", mi_VLREP, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLREPF", mi_VLREP, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLREPG", mi_VLREP, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLREPH", mi_VLREP, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLVGB", mi_VLVG, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLVGF", mi_VLVG, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLVGG", mi_VLVG, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VLVGH", mi_VLVG, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMAEB", mi_VMAE, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMAEF", mi_VMAE, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMAEH", mi_VMAE, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMAHB", mi_VMAH, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMAHF", mi_VMAH, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMAHH", mi_VMAH, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALB", mi_VMAL, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALEB", mi_VMALE, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALEF", mi_VMALE, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALEH", mi_VMALE, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALF", mi_VMAL, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALHB", mi_VMALH, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALHF", mi_VMALH, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALHH", mi_VMALH, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALHW", mi_VMAL, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALOB", mi_VMALO, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALOF", mi_VMALO, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMALOH", mi_VMALO, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMAOB", mi_VMAO, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMAOF", mi_VMAO, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMAOH", mi_VMAO, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMEB", mi_VME, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMEF", mi_VME, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMEH", mi_VME, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMHB", mi_VMH, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMHF", mi_VMH, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMHH", mi_VMH, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLB", mi_VML, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLEB", mi_VMLE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLEF", mi_VMLE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLEH", mi_VMLE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLF", mi_VML, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLHB", mi_VMLH, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLHF", mi_VMLH, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLHH", mi_VMLH, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLHW", mi_VML, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLOB", mi_VMLO, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLOF", mi_VMLO, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMLOH", mi_VMLO, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMNB", mi_VMN, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMNF", mi_VMN, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMNG", mi_VMN, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMNH", mi_VMN, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMNLB", mi_VMNL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMNLF", mi_VMNL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMNLG", mi_VMNL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMNLH", mi_VMNL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMOB", mi_VMO, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMOF", mi_VMO, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMOH", mi_VMO, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMRHB", mi_VMRH, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMRHF", mi_VMRH, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMRHG", mi_VMRH, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMRHH", mi_VMRH, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMRLB", mi_VMRL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMRLF", mi_VMRL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMRLG", mi_VMRL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMRLH", mi_VMRL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMSLG", mi_VMSL, { { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VMXB", mi_VMX, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMXF", mi_VMX, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMXG", mi_VMX, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMXH", mi_VMX, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMXLB", mi_VMXL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMXLF", mi_VMXL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMXLG", mi_VMXL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VMXLH", mi_VMXL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VNOT", mi_VNO, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VONE", mi_VGBM, { { 1, 65535 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKF", mi_VPK, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKG", mi_VPK, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKH", mi_VPK, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKLSF", mi_VPKLS, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKLSFS", mi_VPKLS, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKLSG", mi_VPKLS, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKLSGS", mi_VPKLS, { { 3, 3 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKLSH", mi_VPKLS, { { 3, 1 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKLSHS", mi_VPKLS, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKSF", mi_VPKS, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKSFS", mi_VPKS, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKSG", mi_VPKS, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKSGS", mi_VPKS, { { 3, 3 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKSH", mi_VPKS, { { 3, 1 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPKSHS", mi_VPKS, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VPOPCTB", mi_VPOPCT, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VPOPCTF", mi_VPOPCT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VPOPCTG", mi_VPOPCT, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VPOPCTH", mi_VPOPCT, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "VREPB", mi_VREP, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VREPF", mi_VREP, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VREPG", mi_VREP, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VREPH", mi_VREP, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VREPIB", mi_VREPI, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VREPIF", mi_VREPI, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VREPIG", mi_VREPI, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VREPIH", mi_VREPI, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSB", mi_VS, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSBCBIQ", mi_VSBCBI, { { 4, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSBIQ", mi_VSBI, { { 4, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSCBIB", mi_VSCBI, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSCBIF", mi_VSCBI, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSCBIG", mi_VSCBI, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSCBIH", mi_VSCBI, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSCBIQ", mi_VSCBI, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSEGB", mi_VSEG, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSEGF", mi_VSEG, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSEGH", mi_VSEG, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSF", mi_VS, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSG", mi_VS, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSH", mi_VS, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSQ", mi_VS, { { 3, 4 } } }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_Z13 } }, 
    { { "VSTBRF", mi_VSTBR, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTBRG", mi_VSTBR, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTBRH", mi_VSTBR, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTBRQ", mi_VSTBR, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTERF", mi_VSTER, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTERG", mi_VSTER, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTERH", mi_VSTER, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTRCB", mi_VSTRC, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTRCBS", mi_VSTRC, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTRCF", mi_VSTRC, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTRCFS", mi_VSTRC, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTRCH", mi_VSTRC, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTRCHS", mi_VSTRC, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTRCZB", mi_VSTRC, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTRCZBS", mi_VSTRC, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTRCZF", mi_VSTRC, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTRCZFS", mi_VSTRC, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTRCZH", mi_VSTRC, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTRCZHS", mi_VSTRC, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSTRSB", mi_VSTRS, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTRSF", mi_VSTRS, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTRSH", mi_VSTRS, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSTRSZB", mi_VSTRS, { { 4, 0 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "VSUMB", mi_VSUM, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSUMGF", mi_VSUMG, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSUMGH", mi_VSUMG, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSUMH", mi_VSUM, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSUMQF", mi_VSUMQ, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VSUMQG", mi_VSUMQ, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPHB", mi_VUPH, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPHF", mi_VUPH, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPHH", mi_VUPH, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPLB", mi_VUPL, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPLF", mi_VUPL, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPLHB", mi_VUPLH, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPLHF", mi_VUPLH, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPLHG", mi_VUPLH, { { 2, 1 } } }, { supported_system::UNKNOWN } }, 
    { { "VUPLHW", mi_VUPL, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPLLB", mi_VUPLL, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPLLF", mi_VUPLL, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VUPLLH", mi_VUPLL, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "VZERO", mi_VGBM, { { 0, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WCDGB", mi_VCFPS, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WCDLGB", mi_VCFPL, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WCEFB", mi_VCFPS, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "WCELFB", mi_VCFPL, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "WCFEB", mi_VCSFP, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "WCGDB", mi_VCSFP, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WCLFEB", mi_VCLFP, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z15 } }, 
    { { "WCLGDB", mi_VCLGD, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFADB", mi_VFA, { { 3, 3 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFASB", mi_VFA, { { 3, 2 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFAXB", mi_VFA, { { 3, 4 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCDB", mi_WFC, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFCEDB", mi_VFCE, { { 3, 3 }, { 4, 8 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFCEDBS", mi_VFCE, { { 3, 3 }, { 4, 8 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFCESB", mi_VFCE, { { 3, 2 }, { 4, 8 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCESBS", mi_VFCE, { { 3, 2 }, { 4, 8 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCEXB", mi_VFCE, { { 3, 4 }, { 4, 8 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCEXBS", mi_VFCE, { { 3, 4 }, { 4, 8 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCHDB", mi_VFCH, { { 3, 3 }, { 4, 8 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFCHDBS", mi_VFCH, { { 3, 3 }, { 4, 8 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFCHEDB", mi_VFCHE, { { 3, 3 }, { 4, 8 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFCHEDBS", mi_VFCHE, { { 3, 3 }, { 4, 8 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFCHESB", mi_VFCHE, { { 3, 2 }, { 4, 8 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCHESBS", mi_VFCHE, { { 3, 2 }, { 4, 8 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCHEXB", mi_VFCHE, { { 3, 4 }, { 4, 8 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCHEXBS", mi_VFCHE, { { 3, 4 }, { 4, 8 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCHSB", mi_VFCH, { { 3, 2 }, { 4, 8 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCHSBS", mi_VFCH, { { 3, 2 }, { 4, 8 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCHXB", mi_VFCH, { { 3, 4 }, { 4, 8 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCHXBS", mi_VFCH, { { 3, 4 }, { 4, 8 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCSB", mi_WFC, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFCXB", mi_WFC, { { 3, 4 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFDDB", mi_VFD, { { 3, 3 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFDSB", mi_VFD, { { 3, 2 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFDXB", mi_VFD, { { 3, 4 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFIDB", mi_VFI, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFISB", mi_VFI, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFIXB", mi_VFI, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKDB", mi_WFK, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFKEDB", mi_VFCE, { { 3, 3 }, { 4, 12 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKEDBS", mi_VFCE, { { 3, 3 }, { 4, 12 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKESB", mi_VFCE, { { 3, 2 }, { 4, 12 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKESBS", mi_VFCE, { { 3, 2 }, { 4, 12 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKEXB", mi_VFCE, { { 3, 4 }, { 4, 12 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKEXBS", mi_VFCE, { { 3, 4 }, { 4, 12 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKHDB", mi_VFCH, { { 3, 3 }, { 4, 12 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKHDBS", mi_VFCH, { { 3, 3 }, { 4, 12 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKHEDB", mi_VFCHE, { { 3, 3 }, { 4, 12 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKHEDBS", mi_VFCHE, { { 3, 3 }, { 4, 12 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKHESB", mi_VFCHE, { { 3, 2 }, { 4, 12 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKHESBS", mi_VFCHE, { { 3, 2 }, { 4, 12 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKHEXB", mi_VFCHE, { { 3, 4 }, { 4, 12 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKHEXBS", mi_VFCHE, { { 3, 4 }, { 4, 12 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKHSB", mi_VFCH, { { 3, 2 }, { 4, 12 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKHSBS", mi_VFCH, { { 3, 2 }, { 4, 12 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKHXB", mi_VFCH, { { 3, 4 }, { 4, 12 }, { 5, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKHXBS", mi_VFCH, { { 3, 4 }, { 4, 12 }, { 5, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKSB", mi_WFK, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFKXB", mi_WFK, { { 3, 4 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFLCDB", mi_VFPSO, { { 2, 3 }, { 3, 8 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFLCSB", mi_VFPSO, { { 2, 2 }, { 3, 8 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFLCXB", mi_VFPSO, { { 2, 4 }, { 3, 8 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFLLD", mi_VFLL, { { 2, 3 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFLLS", mi_VFLL, { { 2, 2 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFLNDB", mi_VFPSO, { { 2, 3 }, { 3, 8 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFLNSB", mi_VFPSO, { { 2, 2 }, { 3, 8 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFLNXB", mi_VFPSO, { { 2, 4 }, { 3, 8 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFLPDB", mi_VFPSO, { { 2, 3 }, { 3, 8 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFLPSB", mi_VFPSO, { { 2, 2 }, { 3, 8 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFLPXB", mi_VFPSO, { { 2, 4 }, { 3, 8 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFLRD", mi_VFLR, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFLRX", mi_VFLR, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFMADB", mi_VFMA, { { 4, 8 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFMASB", mi_VFMA, { { 4, 8 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFMAXB", mi_VFMA, { { 4, 8 }, { 5, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFMAXDB", mi_VFMAX, { { 3, 3 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFMAXSB", mi_VFMAX, { { 3, 2 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFMAXXB", mi_VFMAX, { { 3, 4 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFMDB", mi_VFM, { { 3, 3 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFMINDB", mi_VFMIN, { { 3, 3 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFMINSB", mi_VFMIN, { { 3, 2 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFMINXB", mi_VFMIN, { { 3, 4 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFMSB", mi_VFM, { { 3, 2 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFMSDB", mi_VFMS, { { 4, 8 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFMSSB", mi_VFMS, { { 4, 8 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFMSXB", mi_VFMS, { { 4, 8 }, { 5, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFMXB", mi_VFM, { { 3, 4 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFNMADB", mi_VFNMA, { { 4, 8 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFNMASB", mi_VFNMA, { { 4, 8 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFNMAXB", mi_VFNMA, { { 4, 8 }, { 5, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFNMSDB", mi_VFNMS, { { 4, 8 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFNMSSB", mi_VFNMS, { { 4, 8 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFNMSXB", mi_VFNMS, { { 4, 8 }, { 5, 4 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFPSODB", mi_VFPSO, { { 2, 3 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFPSOSB", mi_VFPSO, { { 2, 2 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFPSOXB", mi_VFPSO, { { 2, 4 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFSDB", mi_VFS, { { 2, 3 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFSQDB", mi_VFSQ, { { 2, 3 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFSQSB", mi_VFSQ, { { 2, 2 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFSQXB", mi_VFSQ, { { 2, 4 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFSSB", mi_VFS, { { 2, 2 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFSXB", mi_VFS, { { 2, 4 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFTCIDB", mi_VFTCI, { { 3, 3 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WFTCISB", mi_VFTCI, { { 3, 2 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WFTCIXB", mi_VFTCI, { { 3, 4 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z14 } }, 
    { { "WLDEB", mi_VFLL, { { 2, 2 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "WLEDB", mi_VFLR, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_Z13 } }, 
    { { "XHHR", mi_RXSBG, { { 2, 0 }, { 3, 31 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "XHLR", mi_RXSBG, { { 2, 0 }, { 3, 31 }, { 4, 32 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
    { { "XLHR", mi_RXSBG, { { 2, 32 }, { 3, 63 }, { 4, 32 } } }, { supported_system::UNI | supported_system::SINCE_Z11 } }, 
};
// clang-format on

#ifdef __cpp_lib_ranges

static_assert(std::ranges::is_sorted(mnemonic_codes, {}, [](const auto& code) { return code.first.name(); }));

const mnemonic_code* instruction::find_mnemonic_codes(std::string_view name)
{
    auto it = std::ranges::lower_bound(m_mnemonic_codes, name, {}, &mnemonic_code::name);
    if (it == std::ranges::end(m_mnemonic_codes) || it->get().name() != name)
        return nullptr;
    return &it->get();
}
#else
static_assert(std::is_sorted(std::begin(mnemonic_codes), std::end(mnemonic_codes), [](const auto& l, const auto& r) {
    return l.first.name() < r.first.name();
}));

const mnemonic_code* instruction::find_mnemonic_codes(std::string_view name) const
{
    auto it = std::lower_bound(
        std::begin(m_mnemonic_codes), std::end(m_mnemonic_codes), name, [](const auto& l, const auto& r) {
            return l.get().name() < r;
        });
    if (it == std::end(m_mnemonic_codes) || it->get().name() != name)
        return nullptr;
    return &it->get();
}
#endif

const mnemonic_code& instruction::get_mnemonic_codes(std::string_view name) const
{
    auto result = find_mnemonic_codes(name);
    assert(result);
    return *result;
}

std::span<const std::reference_wrapper<const mnemonic_code>> instruction::all_mnemonic_codes() const
{
    return m_mnemonic_codes;
}

namespace {
bool is_instruction_supported(supported_system instruction_support, system_architecture active_system_arch)
{
    if ((instruction_support & supported_system::UNKNOWN) == supported_system::UNKNOWN)
    {
        return true;
    }

    switch (active_system_arch)
    {
        case system_architecture::UNI: {
            return (instruction_support & supported_system::UNI) == supported_system::UNI;
        }
        case system_architecture::DOS: {
            return (instruction_support & supported_system::DOS) == supported_system::DOS;
        }
        case system_architecture::_370: {
            return (instruction_support & supported_system::_370) == supported_system::_370;
        }
        case system_architecture::XA: {
            return (instruction_support & supported_system::XA) == supported_system::XA;
        }
        case system_architecture::ESA: {
            return (instruction_support & supported_system::ESA) == supported_system::ESA;
        }
        case system_architecture::ZOP:
        case system_architecture::YOP:
        case system_architecture::Z9:
        case system_architecture::Z10:
        case system_architecture::Z11:
        case system_architecture::Z12:
        case system_architecture::Z13:
        case system_architecture::Z14:
        case system_architecture::Z15: {
            instruction_support = instruction_support & 0x0F; // Get the lower bytes representing Z architecture
            return instruction_support == supported_system::NO_Z_SUPPORT ? false
                                                                         : instruction_support <= active_system_arch;
        }
        default:
            return false;
    }
}

template<typename INSTRUCTION_TYPE, typename COMPLETE_INSTRUCTION_SET>
std::vector<std::reference_wrapper<const INSTRUCTION_TYPE>> get_active_instructions(
    system_architecture active_system_arch, COMPLETE_INSTRUCTION_SET& all_instructions)
{
    std::vector<std::reference_wrapper<const INSTRUCTION_TYPE>> active_instructions;

    for (const auto& instruction : all_instructions)
    {
        if (is_instruction_supported(instruction.second, active_system_arch))
        {
            active_instructions.push_back(instruction.first);
        }
    }

    return active_instructions;
}
} // namespace

instruction::instruction(system_architecture arch)
    : m_machine_instructions(get_active_instructions<machine_instruction>(arch, machine_instructions))
    , m_mnemonic_codes(get_active_instructions<mnemonic_code>(arch, mnemonic_codes))
{}