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

std::span<const ca_instruction> instruction::all_ca_instructions() { return ca_instructions; }

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

const assembler_instruction* instruction::find_assembler_instructions(std::string_view instr)
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

const assembler_instruction& instruction::get_assembler_instructions(std::string_view instr)
{
    auto result = find_assembler_instructions(instr);
    assert(result);
    return *result;
}

std::span<const assembler_instruction> instruction::all_assembler_instructions() { return assembler_instructions; }

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
    { { "A", RX_a_2_ux, 510 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "AD", RX_a_2_ux, 1412 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "ADB", RXE_2, 1445 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "ADBR", RRE_2, 1445 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "ADDFRR", RRE_2, 7 }, { supported_system::UNKNOWN } }, 
    { { "ADR", RR_2, 1412 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "ADTR", RRF_a_3, 1491 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "ADTRA", RRF_a_4, 1491 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "AE", RX_a_2_ux, 1412 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "AEB", RXE_2, 1445 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "AEBR", RRE_2, 1445 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "AER", RR_2, 1412 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "AFI", RIL_a_2, 511 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "AG", RXY_a_2, 511 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "AGF", RXY_a_2, 511 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "AGFI", RIL_a_2, 511 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "AGFR", RRE_2, 510 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "AGH", RXY_a_2, 512 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "AGHI", RI_a_2_s, 513 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "AGHIK", RIE_d_3, 511 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "AGR", RRE_2, 510 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "AGRK", RRF_a_3, 510 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "AGSI", SIY_2_ss, 511 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "AH", RX_a_2_ux, 512 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "AHHHR", RRF_a_3, 513 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "AHHLR", RRF_a_3, 513 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "AHI", RI_a_2_s, 512 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "AHIK", RIE_d_3, 511 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "AHY", RXY_a_2, 512 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "AIH", RIL_a_2, 513 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "AL", RX_a_2_ux, 514 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "ALC", RXY_a_2, 515 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "ALCG", RXY_a_2, 515 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "ALCGR", RRE_2, 515 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "ALCR", RRE_2, 515 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "ALFI", RIL_a_2, 514 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "ALG", RXY_a_2, 514 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "ALGF", RXY_a_2, 514 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "ALGFI", RIL_a_2, 514 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "ALGFR", RRE_2, 514 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "ALGHSIK", RIE_d_3, 516 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "ALGR", RRE_2, 514 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "ALGRK", RRF_a_3, 514 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "ALGSI", SIY_2_ss, 516 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "ALHHHR", RRF_a_3, 515 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "ALHHLR", RRF_a_3, 515 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "ALHSIK", RIE_d_3, 516 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "ALR", RR_2, 514 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "ALRK", RRF_a_3, 514 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "ALSI", SIY_2_ss, 516 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "ALSIH", RIL_a_2, 517 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "ALSIHN", RIL_a_2, 517 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "ALY", RXY_a_2, 514 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "AP", SS_b_2, 920 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "AR", RR_2, 510 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "ARK", RRF_a_3, 510 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "ASI", SIY_2_ss, 511 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "AU", RX_a_2_ux, 1413 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "AUR", RR_2, 1413 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "AW", RX_a_2_ux, 1413 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "AWR", RR_2, 1413 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "AXBR", RRE_2, 1445 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "AXR", RR_2, 1412 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "AXTR", RRF_a_3, 1491 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "AXTRA", RRF_a_4, 1491 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "AY", RXY_a_2, 511 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "BAKR", RRE_2, 993 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "BAL", RX_a_2_ux, 519 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "BALR", RR_2, 519 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "BAS", RX_a_2_ux, 520 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "BASR", RR_2, 520 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "BASSM", RX_a_2, 520 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "BC", RX_b_2, 524 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "BCR", RR_2_m, 524 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "BCT", RX_a_2_ux, 525 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "BCTG", RXY_a_2, 525 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "BCTGR", RRE_2, 525 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "BCTR", RR_2, 525 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "BIC", RXY_b_2, 523 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "BPP", SMI_3, 527 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "BPRP", MII_3, 527 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "BRAS", RI_b_2, 530 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "BRASL", RIL_b_2, 530 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "BRC", RI_c_2, 530 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "BRCL", RIL_c_2, 530 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "BRCT", RI_b_2, 531 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "BRCTG", RI_b_2, 531 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "BRCTH", RIL_b_2, 531 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "BRXH", RSI_3, 532 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "BRXHG", RIE_e_3, 532 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "BRXLE", RSI_3, 532 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "BRXLG", RIE_e_3, 532 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "BSA", RRE_2, 989 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "BSG", RRE_2, 995 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "BSM", RR_2, 522 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "BXH", RS_a_3, 526 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "BXHG", RSY_a_3, 526 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "BXLE", RS_a_3, 526 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "BXLEG", RSY_a_3, 526 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "C", RX_a_2_ux, 618 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CD", RX_a_2_ux, 1414 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CDB", RXE_2, 1447 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CDBR", RRE_2, 1447 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CDFBR", RRE_2, 1449 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CDFBRA", RRF_e_4, 1449 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CDFR", RRE_2, 1415 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CDFTR", RRF_e_4, 1496 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CDGBR", RRE_2, 1449 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CDGBRA", RRF_e_4, 1449 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CDGR", RRE_2, 1415 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CDGTR", RRE_2, 1496 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CDGTRA", RRF_e_4, 1496 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CDLFBR", RRF_e_4, 1451 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CDLFTR", RRF_e_4, 1497 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CDLGBR", RRF_e_4, 1451 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CDLGTR", RRF_e_4, 1497 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CDPT", RSL_b_3, 1498 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "CDR", RR_2, 1414 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CDS", RS_a_3, 628 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CDSG", RSY_a_3, 628 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CDSTR", RRE_2, 1500 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CDSY", RSY_a_3, 628 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CDTR", RRE_2, 1494 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CDUTR", RRE_2, 1500 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CDZT", RSL_b_3, 1501 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "CE", RX_a_2_ux, 1414 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CEB", RXE_2, 1447 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CEBR", RRE_2, 1447 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CEDTR", RRE_2, 1495 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CEFBR", RRE_2, 1449 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CEFBRA", RRF_e_4, 1449 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CEFR", RRE_2, 1415 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CEGBR", RRE_2, 1449 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CEGBRA", RRF_e_4, 1449 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CEGR", RRE_2, 1415 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CELFBR", RRF_e_4, 1451 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CELGBR", RRF_e_4, 1451 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CER", RR_2, 1414 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CEXTR", RRE_2, 1495 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CFC", S_1_u, 621 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "CFDBR", RRF_e_3, 1452 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CFDBRA", RRF_e_4, 1452 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CFDR", RRF_e_3, 1415 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CFDTR", RRF_e_4, 1502 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CFEBR", RRF_e_3, 1452 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CFEBRA", RRF_e_4, 1452 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CFER", RRF_e_3, 1415 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CFI", RIL_a_2, 618 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CFXBR", RRF_e_3, 1452 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CFXBRA", RRF_e_4, 1452 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CFXR", RRF_e_3, 1415 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CFXTR", RRF_e_4, 1502 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CG", RXY_a_2, 618 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CGDBR", RRF_e_3, 1452 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CGDBRA", RRF_e_4, 1452 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CGDR", RRF_e_3, 1415 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CGDTR", RRF_e_3, 1501 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CGDTRA", RRF_e_4, 1502 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CGEBR", RRF_e_3, 1452 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CGEBRA", RRF_e_4, 1452 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CGER", RRF_e_3, 1415 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CGF", RXY_a_2, 618 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CGFI", RIL_a_2, 619 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CGFR", RRE_2, 618 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CGFRL", RIL_b_2, 619 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CGH", RXY_a_2, 634 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CGHI", RI_a_2_s, 634 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CGHRL", RIL_b_2, 634 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CGHSI", SIL_2_s, 634 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CGIB", RIS_4, 620 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CGIJ", RIE_c_4, 620 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CGIT", RIE_a_3, 633 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CGR", RRE_2, 618 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CGRB", RRS_4, 619 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CGRJ", RIE_b_4, 620 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CGRL", RIL_b_2, 619 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CGRT", RRF_c_3, 633 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CGXBR", RRF_e_3, 1452 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CGXBRA", RRF_e_4, 1452 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CGXR", RRF_e_3, 1415 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CGXTR", RRF_e_3, 1501 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CGXTRA", RRF_e_4, 1502 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CH", RX_a_2_ux, 634 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CHF", RXY_a_2, 635 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CHHR", RRE_2, 635 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CHHSI", SIL_2_s, 634 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CHI", RI_a_2_s, 634 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CHLR", RRE_2, 635 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CHRL", RIL_b_2, 634 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CHSI", SIL_2_s, 634 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CHY", RXY_a_2, 634 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CIB", RIS_4, 620 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CIH", RIL_a_2, 635 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CIJ", RIE_c_4, 620 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CIT", RIE_a_3, 633 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CKSM", RRE_2, 533 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CL", RX_a_2_ux, 636 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CLC", SS_a_2_u, 636 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CLCL", RR_2, 642 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CLCLE", RS_a_3, 644 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CLCLU", RSY_a_3, 647 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CLFDBR", RRF_e_4, 1455 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLFDTR", RRF_e_4, 1504 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLFEBR", RRF_e_4, 1455 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLFHSI", SIL_2_u, 636 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLFI", RIL_a_2, 636 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CLFIT", RIE_a_3, 640 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLFXBR", RRF_e_4, 1455 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLFXTR", RRF_e_4, 1504 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLG", RXY_a_2, 636 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CLGDBR", RRF_e_4, 1455 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLGDTR", RRF_e_4, 1504 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLGEBR", RRF_e_4, 1455 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLGF", RXY_a_2, 636 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CLGFI", RIL_a_2, 636 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CLGFR", RRE_2, 636 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CLGFRL", RIL_b_2, 637 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLGHRL", RIL_b_2, 637 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLGHSI", SIL_2_u, 636 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLGIB", RIS_4, 638 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLGIJ", RIE_c_4, 638 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLGIT", RIE_a_3, 640 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLGR", RRE_2, 636 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CLGRB", RRS_4, 638 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLGRJ", RIE_b_4, 638 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLGRL", RIL_b_2, 637 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLGRT", RRF_c_3, 639 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLGT", RSY_b_3_ux, 639 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "CLGXBR", RRF_e_4, 1455 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLGXTR", RRF_e_4, 1504 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLHF", RXY_a_2, 641 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLHHR", RRE_2, 641 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLHHSI", SIL_2_u, 636 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLHLR", RRE_2, 641 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLHRL", RIL_b_2, 637 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLI", SI_2_u, 636 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CLIB", RIS_4, 638 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLIH", RIL_a_2, 642 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CLIJ", RIE_c_4, 638 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLIY", SIY_2_su, 636 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CLM", RS_b_3, 641 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CLMH", RSY_b_3_us, 641 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CLMY", RSY_b_3_us, 641 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CLR", RR_2, 636 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CLRB", RRS_4, 638 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLRCH", S_1_u, 367 }, { supported_system::UNI | supported_system::_370 } }, 
    { { "CLRIO", S_1_u, 368 }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "CLRJ", RIE_b_4, 638 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLRL", RIL_b_2, 637 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLRT", RRF_c_3, 639 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CLST", RRE_2, 650 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CLT", RSY_b_3_ux, 639 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "CLY", RXY_a_2, 636 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CMPSC", RRE_2, 654 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CONCS", S_1_u, 263 }, { supported_system::UNI | supported_system::_370 } }, 
    { { "CP", SS_b_2, 921 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CPDT", RSL_b_3, 1505 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "CPSDR", RRF_b_3, 958 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CPXT", RSL_b_3, 1505 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "CPYA", RRE_2, 736 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CR", RR_2, 618 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CRB", RRS_4, 619 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CRDTE", RRF_b_4_opt, 999 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "CRJ", RIE_b_4, 619 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CRL", RIL_b_2, 619 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CRT", RRF_c_3, 633 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "CS", RS_a_3, 628 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CSCH", S_0, 1217 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "CSDTR", RRF_d_3, 1507 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CSG", RSY_a_3, 628 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CSP", RRE_2, 1003 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CSPG", RRE_2, 1003 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CSST", SSF_3_dr, 630 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CSXTR", RRF_d_3, 1507 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CSY", RSY_a_3, 628 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CU12", RRF_c_3_opt, 728 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CU14", RRF_c_3_opt, 732 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CU21", RRF_c_3_opt, 718 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CU24", RRF_c_3_opt, 715 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CU41", RRE_2, 725 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CU42", RRE_2, 722 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CUDTR", RRE_2, 1507 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CUSE", RRE_2, 651 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CUTFU", RRF_c_3_opt, 728 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CUUTF", RRF_c_3_opt, 718 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CUXTR", RRE_2, 1507 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CVB", RX_a_2_ux, 714 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CVBG", RXY_a_2, 714 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CVBY", RXY_a_2, 714 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CVD", RX_a_2_ux, 715 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "CVDG", RXY_a_2, 715 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CVDY", RXY_a_2, 715 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CXBR", RRE_2, 1447 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CXFBR", RRE_2, 1449 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CXFBRA", RRF_e_4, 1449 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CXFR", RRE_2, 1415 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CXFTR", RRF_e_4, 1496 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CXGBR", RRE_2, 1449 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CXGBRA", RRF_e_4, 1449 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CXGR", RRE_2, 1415 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "CXGTR", RRE_2, 1496 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CXGTRA", RRF_e_4, 1496 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CXLFBR", RRF_e_4, 1451 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CXLFTR", RRF_e_4, 1497 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CXLGBR", RRF_e_4, 1451 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CXLGTR", RRF_e_4, 1497 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "CXPT", RSL_b_3, 1498 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "CXR", RRE_2, 1414 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "CXSTR", RRE_2, 1500 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CXTR", RRE_2, 1494 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CXUTR", RRE_2, 1500 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "CXZT", RSL_b_3, 1501 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "CY", RXY_a_2, 618 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "CZDT", RSL_b_3, 1508 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "CZXT", RSL_b_3, 1508 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "D", RX_a_2_ux, 736 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "DD", RX_a_2_ux, 1416 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "DDB", RXE_2, 1457 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "DDBR", RRE_2, 1457 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "DDR", RR_2, 1416 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "DDTR", RRF_a_3, 1509 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "DDTRA", RRF_a_4, 1509 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "DE", RX_a_2_ux, 1416 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "DEB", RXE_2, 1457 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "DEBR", RRE_2, 1457 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "DER", RR_2, 1416 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "DFLTCC", RRF_a_3, 1714 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "DIDBR", RRF_b_4, 1458 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "DIEBR", RRF_b_4, 1458 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "DISCS", S_1_u, 265 }, { supported_system::UNI | supported_system::_370 } }, 
    { { "DL", RXY_a_2, 737 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "DLG", RXY_a_2, 737 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "DLGR", RRE_2, 737 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "DLR", RRE_2, 737 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "DP", SS_b_2, 921 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "DR", RR_2, 736 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "DSG", RXY_a_2, 738 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "DSGF", RXY_a_2, 738 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "DSGFR", RRE_2, 738 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "DSGR", RRE_2, 738 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "DXBR", RRE_2, 1457 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "DXR", RRE_2, 1416 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "DXTR", RRF_a_3, 1509 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "DXTRA", RRF_a_4, 1509 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "EAR", RRE_2, 741 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "ECAG", RSY_a_3, 741 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "ECCTR", RRE_2, 39 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "ECPGA", RRE_2, 39 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "ECTG", SSF_3_dr, 744 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "ED", SS_a_2_u, 922 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "EDMK", SS_a_2_u, 925 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "EEDTR", RRE_2, 1511 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "EEXTR", RRE_2, 1511 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "EFPC", RRE_1, 958 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "EPAIR", RRE_1, 1006 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "EPAR", RRE_1, 1006 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "EPCTR", RRE_2, 39 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "EPSW", RRE_2, 745 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "EREG", RRE_2, 1007 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "EREGG", RRE_2, 1007 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "ESAIR", RRE_1, 1007 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "ESAR", RRE_1, 1006 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "ESDTR", RRE_2, 1511 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "ESEA", RRE_1, 1006 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "ESTA", RRE_2, 1008 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "ESXTR", RRE_2, 1511 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "ETND", RRE_1, 745 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "EX", RX_a_2_ux, 740 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "EXRL", RIL_b_2, 740 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "FIDBR", RRF_e_3, 1462 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "FIDBRA", RRF_e_4, 1462 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "FIDR", RRE_2, 1419 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "FIDTR", RRF_e_4, 1514 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "FIEBR", RRF_e_3, 1462 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "FIEBRA", RRF_e_4, 1462 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "FIER", RRE_2, 1419 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "FIXBR", RRF_e_3, 1462 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "FIXBRA", RRF_e_4, 1462 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "FIXR", RRE_2, 1419 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "FIXTR", RRF_e_4, 1514 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "FLOGR", RRE_2, 746 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "HDR", RR_2, 1417 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "HDV", S_1_u, 129 }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "HER", RR_2, 1417 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "HIO", S_1_u, 129 }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "HSCH", S_0, 1218 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "IAC", RRE_1, 1011 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "IC", RX_a_2_ux, 746 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "ICM", RS_b_3, 746 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "ICMH", RSY_b_3_us, 746 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "ICMY", RSY_b_3_us, 746 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "ICY", RXY_a_2, 746 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "IDTE", RRF_b_4_opt, 1014 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "IEDTR", RRF_b_3, 1512 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "IEXTR", RRF_b_3, 1512 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "IIHF", RIL_a_2, 747 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "IIHH", RI_a_2_u, 747 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "IIHL", RI_a_2_u, 747 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "IILF", RIL_a_2, 747 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "IILH", RI_a_2_u, 747 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "IILL", RI_a_2_u, 747 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "IPK", S_0, 1012 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "IPM", RRE_1, 748 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "IPTE", RRF_a_4_opt, 1019 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "IRBM", RRE_2, 1012 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "ISK", RR_2, 268 }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "ISKE", RRE_2, 1012 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "IVSK", RRE_2, 1013 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "KDB", RXE_2, 1448 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "KDBR", RRE_2, 1448 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "KDSA", RRE_2, 1700 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "KDTR", RRE_2, 1495 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "KEB", RXE_2, 1448 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "KEBR", RRE_2, 1448 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "KIMD", RRE_2, 672 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "KLMD", RRE_2, 685 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "KM", RRE_2, 537 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "KMA", RRF_b_3, 562 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "KMAC", RRE_2, 703 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "KMC", RRE_2, 537 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "KMCTR", RRF_b_3, 591 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "KMF", RRE_2, 576 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "KMO", RRE_2, 604 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "KXBR", RRE_2, 1448 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "KXTR", RRE_2, 1495 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "L", RX_a_2_ux, 748 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LA", RX_a_2_ux, 750 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LAA", RSY_a_3, 752 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LAAG", RSY_a_3, 752 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LAAL", RSY_a_3, 752 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LAALG", RSY_a_3, 752 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LAE", RX_a_2_ux, 750 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LAEY", RXY_a_2, 750 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LAM", RS_a_3, 749 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LAMY", RSY_a_3, 749 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "LAN", RSY_a_3, 753 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LANG", RSY_a_3, 753 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LAO", RSY_a_3, 754 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LAOG", RSY_a_3, 754 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LARL", RIL_b_2, 751 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LASP", SSE_2, 1023 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "LAT", RXY_a_2, 755 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "LAX", RSY_a_3, 753 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LAXG", RSY_a_3, 753 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LAY", RXY_a_2, 750 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "LB", RXY_a_2, 756 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "LBH", RXY_a_2, 756 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LBR", RRE_2, 756 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LCBB", RXE_3_xm, 757 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "LCCTL", S_1_u, 40 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LCDBR", RRE_2, 1461 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LCDFR", RRE_2, 959 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LCDR", RR_2, 1418 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LCEBR", RRE_2, 1461 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LCER", RR_2, 1418 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LCGFR", RRE_2, 757 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LCGR", RRE_2, 757 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LCR", RR_2, 756 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LCTL", RS_a_3, 1032 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LCTLG", RSY_a_3, 1032 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LCXBR", RRE_2, 1461 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LCXR", RRE_2, 1418 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LD", RX_a_2_ux, 959 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LDE", RXE_2, 1419 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LDEB", RRE_2, 1464 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LDEBR", RRE_2, 1463 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LDER", RRE_2, 1419 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LDETR", RRF_d_3, 1517 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LDGR", RRE_2, 962 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LDR", RR_2, 959 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LDXBR", RRE_2, 1465 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LDXBRA", RRF_e_4, 1465 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LDXR", RR_2, 1421 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LDXTR", RRF_e_4, 1518 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LDY", RXY_a_2, 959 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "LE", RX_a_2_ux, 959 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LEDBR", RRE_2, 1465 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LEDBRA", RRF_e_4, 1465 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LEDR", RR_2, 1421 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LEDTR", RRF_e_4, 1518 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LER", RR_2, 959 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LEXBR", RRE_2, 1465 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LEXBRA", RRF_e_4, 1465 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LEXR", RRE_2, 1421 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LEY", RXY_a_2, 959 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "LFAS", S_1_u, 960 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LFH", RXY_a_2, 762 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LFHAT", RXY_a_2, 762 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "LFPC", S_1_u, 959 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LG", RXY_a_2, 748 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LGAT", RXY_a_2, 755 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "LGB", RXY_a_2, 756 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "LGBR", RRE_2, 756 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LGDR", RRE_2, 962 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LGF", RXY_a_2, 748 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LGFI", RIL_a_2, 748 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LGFR", RRE_2, 748 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LGFRL", RIL_b_2, 748 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LGG", RXY_a_2, 758 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "LGH", RXY_a_2, 760 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LGHI", RI_a_2_s, 760 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LGHR", RRE_2, 760 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LGHRL", RIL_b_2, 760 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LGR", RRE_2, 748 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LGRL", RIL_b_2, 748 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LGSC", RXY_a_2, 759 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "LH", RX_a_2_ux, 760 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LHH", RXY_a_2, 761 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LHI", RI_a_2_s, 760 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LHR", RRE_2, 760 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LHRL", RIL_b_2, 760 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LHY", RXY_a_2, 760 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "LLC", RXY_a_2, 763 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LLCH", RXY_a_2, 764 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LLCR", RRE_2, 763 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LLGC", RXY_a_2, 763 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LLGCR", RRE_2, 763 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LLGF", RXY_a_2, 762 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LLGFAT", RXY_a_2, 763 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "LLGFR", RRE_2, 762 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LLGFRL", RIL_b_2, 762 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LLGFSG", RXY_a_2, 758 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "LLGH", RXY_a_2, 764 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LLGHR", RRE_2, 764 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LLGHRL", RIL_b_2, 764 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LLGT", RXY_a_2, 766 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LLGTAT", RXY_a_2, 766 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "LLGTR", RRE_2, 765 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LLH", RXY_a_2, 764 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LLHH", RXY_a_2, 765 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LLHR", RRE_2, 764 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LLHRL", RIL_b_2, 764 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LLIHF", RIL_a_2, 765 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LLIHH", RI_a_2_u, 765 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LLIHL", RI_a_2_u, 765 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LLILF", RIL_a_2, 765 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LLILH", RI_a_2_u, 765 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LLILL", RI_a_2_u, 765 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LLZRGF", RXY_a_2, 763 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "LM", RS_a_3, 766 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LMD", SS_e_4_rb, 767 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LMG", RSY_a_3, 766 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LMH", RSY_a_3, 767 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LMY", RSY_a_3, 766 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "LNDBR", RRE_2, 1464 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LNDFR", RRE_2, 962 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LNDR", RR_2, 1420 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LNEBR", RRE_2, 1464 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LNER", RR_2, 1420 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LNGFR", RRE_2, 768 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LNGR", RRE_2, 767 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LNR", RR_2, 767 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LNXBR", RRE_2, 1464 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LNXR", RRE_2, 1420 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LOC", RSY_b_3_su, 768 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LOCFH", RSY_b_3_su, 768 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "LOCFHR", RRF_c_3, 768 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "LOCG", RSY_b_3_su, 768 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LOCGHI", RIE_g_3, 761 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "LOCGR", RRF_c_3, 768 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LOCHHI", RIE_g_3, 761 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "LOCHI", RIE_g_3, 761 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "LOCR", RRF_c_3, 768 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LPCTL", S_1_u, 41 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LPD", SSF_3_rd, 769 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LPDBR", RRE_2, 1465 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LPDFR", RRE_2, 962 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LPDG", SSF_3_rd, 769 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "LPDR", RR_2, 1420 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LPEBR", RRE_2, 1465 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LPER", RR_2, 1420 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LPGFR", RRE_2, 771 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LPGR", RRE_2, 771 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LPP", S_1_u, 11 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LPQ", RXY_a_2, 770 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LPR", RR_2, 771 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LPSW", SI_1, 1036 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LPSWE", S_1_u, 1037 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LPTEA", RRF_b_4, 1032 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LPXBR", RRE_2, 1465 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LPXR", RRE_2, 1420 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LR", RR_2, 748 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LRA", RX_a_2_ux, 1038 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LRAG", RXY_a_2, 1038 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LRAY", RXY_a_2, 1038 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "LRDR", RR_2, 1421 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LRER", RR_2, 1421 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LRL", RIL_b_2, 748 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LRV", RXY_a_2, 771 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LRVG", RXY_a_2, 771 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LRVGR", RRE_2, 771 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LRVH", RXY_a_2, 771 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LRVR", RRE_2, 771 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LSCTL", S_1_u, 42 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LT", RXY_a_2, 755 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LTDBR", RRE_2, 1461 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LTDR", RR_2, 1417 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LTDTR", RRE_2, 1513 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LTEBR", RRE_2, 1461 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LTER", RR_2, 1417 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LTG", RXY_a_2, 755 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LTGF", RXY_a_2, 755 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "LTGFR", RRE_2, 754 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LTGR", RRE_2, 754 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LTR", RR_2, 754 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "LTXBR", RRE_2, 1461 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LTXR", RRE_2, 1418 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LTXTR", RRE_2, 1513 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LURA", RRE_2, 1042 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LURAG", RRE_2, 1042 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "LXD", RXE_2, 1419 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LXDB", RRE_2, 1464 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LXDBR", RRE_2, 1463 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LXDR", RRE_2, 1419 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LXDTR", RRF_d_3, 1517 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "LXE", RXE_2, 1419 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LXEB", RRE_2, 1464 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LXEBR", RRE_2, 1463 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LXER", RRE_2, 1419 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LXR", RRE_2, 959 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LY", RXY_a_2, 748 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "LZDR", RRE_1, 963 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LZER", RRE_1, 963 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "LZRF", RXY_a_2, 755 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "LZRG", RXY_a_2, 755 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "LZXR", RRE_1, 963 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "M", RX_a_2_ux, 788 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MAD", RXF_3_x, 1423 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "MADB", RXF_3_x, 1468 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MADBR", RRD_3, 1468 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MADR", RRD_3, 1423 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "MAE", RXF_3_x, 1423 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "MAEB", RXF_3_x, 1468 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MAEBR", RRD_3, 1468 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MAER", RRD_3, 1423 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "MAY", RXF_3_x, 1424 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MAYH", RXF_3_x, 1424 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MAYHR", RRD_3, 1424 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MAYL", RXF_3_x, 1424 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MAYLR", RRD_3, 1424 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MAYR", RRD_3, 1424 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MC", SI_2_s, 772 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MD", RX_a_2_ux, 1422 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MDB", RXE_2, 1467 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MDBR", RRE_2, 1467 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MDE", RX_a_2_ux, 1422 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MDEB", RXE_2, 1467 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MDEBR", RRE_2, 1467 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MDER", RR_2, 1421 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MDR", RR_2, 1421 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MDTR", RRF_a_3, 1519 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MDTRA", RRF_a_4, 1520 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "ME", RX_a_2_ux, 1422 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MEE", RXE_2, 1422 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MEEB", RXE_2, 1467 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MEEBR", RRE_2, 1467 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MEER", RRE_2, 1421 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MER", RR_2, 1421 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MFY", RXY_a_2, 788 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "MG", RXY_a_2, 788 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "MGH", RXY_a_2, 789 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "MGHI", RI_a_2_s, 789 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "MGRK", RRF_a_3, 788 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "MH", RX_a_2_ux, 789 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MHI", RI_a_2_s, 789 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MHY", RXY_a_2, 789 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "ML", RXY_a_2, 790 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MLG", RXY_a_2, 790 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "MLGR", RRE_2, 790 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "MLR", RRE_2, 790 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MP", SS_b_2, 926 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MR", RR_2, 788 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MS", RX_a_2_ux, 791 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MSC", RXY_a_2, 791 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "MSCH", S_1_u, 1219 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "MSD", RXF_3_x, 1423 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "MSDB", RXF_3_x, 1468 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MSDBR", RRD_3, 1468 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MSDR", RRD_3, 1423 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "MSE", RXF_3_x, 1423 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "MSEB", RXF_3_x, 1468 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MSEBR", RRD_3, 1468 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MSER", RRD_3, 1423 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "MSFI", RIL_a_2, 791 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "MSG", RXY_a_2, 791 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "MSGC", RXY_a_2, 791 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "MSGF", RXY_a_2, 791 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "MSGFI", RIL_a_2, 791 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "MSGFR", RRE_2, 791 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "MSGR", RRE_2, 791 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "MSGRKC", RRF_a_3, 791 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "MSR", RRE_2, 791 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MSRKC", RRF_a_3, 791 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "MSTA", RRE_1, 1043 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MSY", RXY_a_2, 791 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "MVC", SS_a_2_u, 773 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MVCDK", SSE_2, 1048 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MVCIN", SS_a_2_u, 774 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MVCK", SS_d_3, 1049 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "MVCL", RR_2, 774 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MVCLE", RS_a_3, 778 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MVCLU", RSY_a_3, 781 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "MVCOS", SSF_3_dr, 1050 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MVCP", SS_d_3, 1046 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "MVCRL", SSE_2, 788 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "MVCS", SS_d_3, 1046 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "MVCSK", SSE_2, 1053 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MVGHI", SIL_2_s, 773 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "MVHHI", SIL_2_s, 773 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "MVHI", SIL_2_s, 773 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "MVI", SI_2_u, 773 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MVIY", SIY_2_su, 773 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "MVN", SS_a_2_u, 785 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MVO", SS_b_2, 786 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MVPG", RRE_2, 1044 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MVST", RRE_2, 785 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MVZ", SS_a_2_u, 787 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MXBR", RRE_2, 1467 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MXD", RX_a_2_ux, 1422 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MXDB", RXE_2, 1467 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MXDBR", RRE_2, 1467 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "MXDR", RR_2, 1421 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MXR", RR_2, 1421 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "MXTR", RRF_a_3, 1519 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MXTRA", RRF_a_4, 1520 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "MY", RXF_3_x, 1426 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MYH", RXF_3_x, 1426 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MYHR", RRD_3, 1426 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MYL", RXF_3_x, 1426 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MYLR", RRD_3, 1426 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "MYR", RRD_3, 1426 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "N", RX_a_2_ux, 517 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "NC", SS_a_2_u, 518 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "NCGRK", RRF_a_3, 522 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "NCRK", RRF_a_3, 522 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "NG", RXY_a_2, 517 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "NGR", RRE_2, 517 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "NGRK", RRF_a_3, 517 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "NI", SI_2_u, 517 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "NIAI", IE_2, 792 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "NIHF", RIL_a_2, 518 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "NIHH", RI_a_2_u, 518 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "NIHL", RI_a_2_u, 518 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "NILF", RIL_a_2, 519 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "NILH", RI_a_2_u, 519 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "NILL", RI_a_2_u, 519 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "NIY", SIY_2_su, 518 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "NNGRK", RRF_a_3, 796 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "NNRK", RRF_a_3, 796 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "NOGRK", RRF_a_3, 799 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "NORK", RRF_a_3, 799 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "NR", RR_2, 517 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "NRK", RRF_a_3, 517 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "NTSTG", RXY_a_2, 794 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "NXGRK", RRF_a_3, 799 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "NXRK", RRF_a_3, 799 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "NY", RXY_a_2, 517 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "O", RX_a_2_ux, 794 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "OC", SS_a_2_u, 795 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "OCGRK", RRF_a_3, 802 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "OCRK", RRF_a_3, 802 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "OG", RXY_a_2, 795 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "OGR", RRE_2, 794 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "OGRK", RRF_a_3, 794 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "OI", SI_2_u, 795 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "OIHF", RIL_a_2, 796 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "OIHH", RI_a_2_u, 796 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "OIHL", RI_a_2_u, 796 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "OILF", RIL_a_2, 796 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "OILH", RI_a_2_u, 796 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "OILL", RI_a_2_u, 796 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "OIY", SIY_2_su, 795 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "OR", RR_2, 794 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "ORK", RRF_a_3, 794 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "OY", RXY_a_2, 794 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "PACK", SS_b_2, 796 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "PALB", RRE_0, 1098 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "PC", S_1_u, 1072 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "PCC", RRE_0, 799 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "PCKMO", RRE_0, 1056 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "PFD", RXY_b_2, 843 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "PFDRL", RIL_c_2, 843 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "PFMF", RRE_2, 1059 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "PFPO", E_0, 963 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "PGIN", RRE_2, 1054 }, { supported_system::UNKNOWN } }, 
    { { "PGOUT", RRE_2, 1055 }, { supported_system::UNKNOWN } }, 
    { { "PKA", SS_f_2, 797 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "PKU", SS_f_2, 798 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "PLO", SS_e_4_br, 815 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "POPCNT", RRF_c_3_opt, 853 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "PPA", RRF_c_3, 829 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "PPNO", RRE_2, 830 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "PR", E_0, 1085 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "PRNO", RRE_2, 830 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "PT", RRE_2, 1089 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "PTF", RRE_1, 1071 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "PTFF", E_0, 1063 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "PTI", RRE_2, 1089 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "PTLB", S_0, 1098 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "QADTR", RRF_b_4, 1521 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "QAXTR", RRF_b_4, 1521 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "QCTRI", S_1_u, 43 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "QSI", S_1_u, 45 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "RCHP", S_0, 1221 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "RISBG", RIE_f_5, 847 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "RISBGN", RIE_f_5, 847 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "RISBGNZ", RIE_f_5, 860 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "RISBGZ", RIE_f_5, 858 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "RISBHG", RIE_f_5, 848 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "RISBHGZ", RIE_f_5, 860 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "RISBLG", RIE_f_5, 849 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "RISBLGZ", RIE_f_5, 860 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "RLL", RSY_a_3, 845 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "RLLG", RSY_a_3, 845 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "RNSBG", RIE_f_5, 845 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "RNSBGT", RIE_f_5, 845 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "ROSBG", RIE_f_5, 846 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "ROSBGT", RIE_f_5, 858 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "RP", S_1_u, 1099 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "RRB", S_1_u, 295 }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "RRBE", RRE_2, 1098 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "RRBM", RRE_2, 1099 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "RRDTR", RRF_b_4, 1524 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "RRXTR", RRF_b_4, 1524 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "RSCH", S_0, 1222 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "RXSBG", RIE_f_5, 846 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "RXSBGT", RIE_f_5, 846 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "S", RX_a_2_ux, 872 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SAC", S_1_u, 1102 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "SACF", S_1_u, 1102 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SAL", S_0, 1224 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "SAM24", E_0, 854 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SAM31", E_0, 854 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SAM64", E_0, 854 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SAR", RRE_2, 854 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SCCTR", RRE_2, 46 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "SCHM", S_0, 1225 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "SCK", S_1_u, 1103 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SCKC", S_1_u, 1104 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SCKPF", E_0, 1105 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SD", RX_a_2_ux, 1428 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SDB", RXE_2, 1470 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SDBR", RRE_2, 1470 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SDR", RR_2, 1428 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SDTR", RRF_a_3, 1527 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "SDTRA", RRF_a_4, 1527 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SE", RX_a_2_ux, 1428 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SEB", RXE_2, 1470 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SEBR", RRE_2, 1470 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SELFHR", RRF_a_4, 864 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "SELGR", RRF_a_4, 864 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "SELR", RRF_a_4, 864 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "SER", RR_2, 1428 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SFASR", RRE_1, 976 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "SFPC", RRE_1, 975 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SG", RXY_a_2, 872 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SGF", RXY_a_2, 872 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SGFR", RRE_2, 871 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SGH", RXY_a_2, 872 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "SGR", RRE_2, 871 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SGRK", RRF_a_3, 872 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SH", RX_a_2_ux, 872 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SHHHR", RRF_a_3, 873 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SHHLR", RRF_a_3, 873 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SHY", RXY_a_2, 872 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "SIE", S_1_u, 7 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "SIGP", RS_a_3, 1115 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "SIO", S_1_u, 129 }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "SIOF", S_1_u, 129 }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "SL", RX_a_2_ux, 874 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SLA", RS_a_2, 856 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SLAG", RSY_a_3, 856 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SLAK", RSY_a_3, 856 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SLB", RXY_a_2, 875 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SLBG", RXY_a_2, 875 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SLBGR", RRE_2, 875 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SLBR", RRE_2, 875 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SLDA", RS_a_2, 855 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SLDL", RS_a_2, 856 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SLDT", RXF_3_x, 1526 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "SLFI", RIL_a_2, 874 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "SLG", RXY_a_2, 874 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SLGF", RXY_a_2, 874 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SLGFI", RIL_a_2, 874 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "SLGFR", RRE_2, 873 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SLGR", RRE_2, 873 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SLGRK", RRF_a_3, 873 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SLHHHR", RRF_a_3, 875 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SLHHLR", RRF_a_3, 875 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SLL", RS_a_2, 857 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SLLG", RSY_a_3, 857 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SLLK", RSY_a_3, 857 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SLR", RR_2, 873 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SLRK", RRF_a_3, 873 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SLXT", RXF_3_x, 1526 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "SLY", RXY_a_2, 874 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "SORTL", RRE_2, 19 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "SP", SS_b_2, 927 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SPCTR", RRE_2, 47 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "SPKA", S_1_u, 1106 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SPM", RR_1, 855 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SPT", S_1_u, 1105 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SPX", S_1_u, 1105 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "SQD", RXE_2, 1427 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SQDB", RXE_2, 1470 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SQDBR", RRE_2, 1470 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SQDR", RRE_2, 1427 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "SQE", RXE_2, 1427 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SQEB", RXE_2, 1470 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SQEBR", RRE_2, 1470 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SQER", RRE_2, 1427 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "SQXBR", RRE_2, 1470 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SQXR", RRE_2, 1427 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SR", RR_2, 871 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SRA", RS_a_2, 859 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SRAG", RSY_a_3, 859 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SRAK", RSY_a_3, 859 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SRDA", RS_a_2, 858 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SRDL", RS_a_2, 858 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SRDT", RXF_3_x, 1526 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "SRK", RRF_a_3, 871 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SRL", RS_a_2, 860 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SRLG", RSY_a_3, 860 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "SRLK", RSY_a_3, 860 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SRNM", S_1_u, 975 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SRNMB", S_1_u, 975 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SRNMT", S_1_u, 975 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "SRP", SS_c_3, 926 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SRST", RRE_2, 850 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SRSTU", RRE_2, 852 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "SRXT", RXF_3_x, 1526 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "SSAIR", RRE_1, 1107 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "SSAR", RRE_1, 1107 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "SSCH", S_1_u, 1227 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "SSK", RR_2, 304 }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "SSKE", RRF_c_3_opt, 1112 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "SSM", SI_1, 1115 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "ST", RX_a_2_ux, 860 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STAM", RS_a_3, 861 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "STAMY", RSY_a_3, 861 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "STAP", S_1_u, 1118 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "STC", RX_a_2_ux, 862 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STCH", RXY_a_2, 862 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "STCK", S_1_u, 863 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STCKC", S_1_u, 1117 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STCKE", S_1_u, 864 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "STCKF", S_1_u, 863 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "STCM", RS_b_3, 862 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STCMH", RSY_b_3_us, 862 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "STCMY", RSY_b_3_us, 862 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "STCPS", S_1_u, 1228 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "STCRW", S_1_u, 1229 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "STCTG", RSY_a_3, 1117 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "STCTL", RS_a_3, 1117 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STCY", RXY_a_2, 862 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "STD", RX_a_2_ux, 976 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STDY", RXY_a_2, 977 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "STE", RX_a_2_ux, 976 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STEY", RXY_a_2, 977 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "STFH", RXY_a_2, 868 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "STFL", S_1_u, 1120 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "STFLE", S_1_s, 866 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "STFPC", S_1_u, 977 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "STG", RXY_a_2, 861 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "STGRL", RIL_b_2, 861 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "STGSC", RXY_a_2, 867 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "STH", RX_a_2_ux, 867 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STHH", RXY_a_2, 868 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "STHRL", RIL_b_2, 868 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "STHY", RXY_a_2, 868 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "STIDC", S_1_u, 129 }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "STIDP", S_1_u, 1118 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STM", RS_a_3, 869 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STMG", RSY_a_3, 869 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "STMH", RSY_a_3, 869 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "STMY", RSY_a_3, 869 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "STNSM", SI_2_u, 1146 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STOC", RSY_b_3_su, 869 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "STOCFH", RSY_b_3_su, 870 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "STOCG", RSY_b_3_su, 869 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "STOSM", SI_2_u, 1146 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STPQ", RXY_a_2, 870 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "STPT", S_1_u, 1120 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "STPX", S_1_u, 1121 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "STRAG", SSE_2, 1121 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "STRL", RIL_b_2, 861 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "STRV", RXY_a_2, 871 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "STRVG", RXY_a_2, 871 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "STRVH", RXY_a_2, 871 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "STSCH", S_1_u, 1230 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "STSI", S_1_u, 1122 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "STURA", RRE_2, 1147 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "STURG", RRE_2, 1147 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "STY", RXY_a_2, 861 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "SU", RX_a_2_ux, 1429 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SUR", RR_2, 1429 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SVC", I_1, 876 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SW", RX_a_2_ux, 1429 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SWR", RR_2, 1429 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SXBR", RRE_2, 1470 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "SXR", RR_2, 1428 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "SXTR", RRF_a_3, 1527 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "SXTRA", RRF_a_4, 1527 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "SY", RXY_a_2, 872 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "TABORT", S_1_u, 878 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "TAM", E_0, 876 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TAR", RRE_2, 1147 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TB", RRE_2, 1149 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "TBDR", RRF_e_3, 956 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TBEDR", RRF_e_3, 956 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TBEGIN", SIL_2_s, 879 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "TBEGINC", SIL_2_s, 883 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "TCDB", RXE_2, 1471 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TCEB", RXE_2, 1471 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TCH", S_1_u, 384 }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "TCXB", RXE_2, 1471 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TDCDT", RXE_2, 1528 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "TDCET", RXE_2, 1528 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "TDCXT", RXE_2, 1528 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "TDGDT", RXE_2, 1529 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "TDGET", RXE_2, 1529 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "TDGXT", RXE_2, 1529 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "TEND", S_0, 885 }, { supported_system::UNI | supported_system::SINCE_ZS6 } }, 
    { { "THDER", RRE_2, 955 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "THDR", RRE_2, 955 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TIO", S_1_u, 385 }, { supported_system::UNI | supported_system::_370 | supported_system::DOS } }, 
    { { "TM", SI_2_u, 877 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "TMH", RI_a_2_u, 877 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TMHH", RI_a_2_u, 877 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "TMHL", RI_a_2_u, 877 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "TML", RI_a_2_u, 877 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TMLH", RI_a_2_u, 877 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TMLL", RI_a_2_u, 877 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TMY", SIY_2_su, 877 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "TP", RSL_a_1, 928 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "TPEI", RRE_2, 1151 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "TPI", S_1_u, 1231 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "TPROT", SSE_2, 1152 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS1 } }, 
    { { "TR", SS_a_2_u, 886 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "TRACE", RS_a_3, 1155 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "TRACG", RSY_a_3, 1155 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "TRAP2", E_0, 1156 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TRAP4", S_1_u, 1156 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TRE", RRE_2, 893 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "TROO", RRF_c_3_opt, 895 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "TROT", RRF_c_3_opt, 895 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "TRT", SS_a_2_u, 887 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "TRTE", RRF_c_3_opt, 887 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "TRTO", RRF_c_3_opt, 895 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "TRTR", SS_a_2_u, 892 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "TRTRE", RRF_c_3_opt, 888 }, { supported_system::UNI | supported_system::SINCE_ZS4 } }, 
    { { "TRTT", RRF_c_3_opt, 895 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "TS", SI_1, 876 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "TSCH", S_1_u, 1232 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "UNPK", SS_b_2, 900 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "UNPKA", SS_a_2_u, 901 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "UNPKU", SS_a_2_u, 902 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "UPT", E_0, 903 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::SINCE_ZS1 } }, 
    { { "VA", VRR_c_4, 1557 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VAC", VRR_d_5, 1558 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VACC", VRR_c_4, 1558 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VACCC", VRR_d_5, 1559 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VACD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VACE", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VACRS", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VACSV", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VAD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VADS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VAE", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VAES", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VAP", VRI_f_5, 1643 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VAS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VAVG", VRR_c_4, 1560 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VAVGL", VRR_c_4, 1560 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VBPERM", VRR_c_3, 1536 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VC", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCDS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCE", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCEQ", VRR_b_5, 1561 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VCES", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCFPL", VRR_a_5, 1643 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VCFPS", VRR_a_5, 1641 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VCH", VRR_b_5, 1562 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VCHL", VRR_b_5, 1563 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VCKSM", VRR_c_3, 1560 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VCLFP", VRR_a_5, 1611 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VCLGD", VRR_a_5, 1611 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VCLZ", VRR_a_3, 1564 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VCOVM", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCP", VRR_h_3, 1644 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VCS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCSFP", VRR_a_5, 1644 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VCTZ", VRR_a_3, 1564 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VCVB", VRR_i_3, 1645 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VCVBG", VRR_i_3, 1645 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VCVD", VRI_i_4, 1646 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VCVDG", VRI_i_4, 1646 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VCVM", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VCZVM", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VDD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VDDS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VDE", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VDES", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VDP", VRI_f_5, 1648 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VEC", VRR_a_3, 1561 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VECL", VRR_a_3, 1561 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VERIM", VRI_d_5, 1576 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VERLL", VRS_a_4, 1575 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VERLLV", VRR_c_4, 1575 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VESL", VRS_a_4, 1577 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VESLV", VRR_c_4, 1577 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VESRA", VRS_a_4, 1577 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VESRAV", VRR_c_4, 1577 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VESRL", VRS_a_4, 1578 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VESRLV", VRR_c_4, 1578 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFA", VRR_c_5, 1595 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFAE", VRR_b_5_opt, 1585 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFCE", VRR_c_6, 1601 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFCH", VRR_c_6, 1603 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFCHE", VRR_c_6, 1605 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFD", VRR_c_5, 1613 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFEE", VRR_b_5_opt, 1587 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFENE", VRR_b_5_opt, 1588 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFI", VRR_a_5, 1615 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFLL", VRR_a_4, 1617 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VFLR", VRR_a_5, 1618 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VFM", VRR_c_5, 1631 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFMA", VRR_e_6, 1633 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFMAX", VRR_c_6, 1619 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VFMIN", VRR_c_6, 1625 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VFMS", VRR_e_6, 1633 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFNMA", VRR_e_6, 1633 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VFNMS", VRR_e_6, 1633 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VFPSO", VRR_a_5, 1635 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFS", VRR_c_5, 1637 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFSQ", VRR_a_4, 1636 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VFTCI", VRI_e_5, 1638 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VGBM", VRI_a_2, 1537 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VGEF", VRV_3, 1536 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VGEG", VRV_3, 1536 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VGFM", VRR_c_4, 1565 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VGFMA", VRR_d_5, 1566 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VGM", VRI_b_4, 1537 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VISTR", VRR_a_4_opt, 1589 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VL", VRX_3_opt, 1538 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VLBB", VRX_3, 1542 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLBIX", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLBR", VRX_3, 1563 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VLBRREP", VRX_3, 1562 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VLC", VRR_a_3, 1566 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLCVM", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLEB", VRX_3, 1538 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLEBRG", VRX_3, 1561 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VLEBRH", VRX_3, 1561 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VLEF", VRX_3, 1539 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLEG", VRX_3, 1539 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLEH", VRX_3, 1539 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLEIB", VRI_a_3, 1539 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLEIF", VRI_a_3, 1539 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLEIG", VRI_a_3, 1539 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLEIH", VRI_a_3, 1539 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLELD", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLELE", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLER", VRX_3, 1564 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS9 } }, 
    { { "VLGV", VRS_c_4, 1539 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLH", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLI", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLID", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLINT", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLIP", VRI_h_3, 1649 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VLL", VRS_b_3, 1543 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLLEBRZ", VRX_3, 1562 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VLLEZ", VRX_3, 1540 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLM", VRS_a_4_opt, 1541 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VLMD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLP", VRR_a_3, 1566 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLR", VRR_a_2, 1538 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VLREP", VRX_3, 1538 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLRL", VSI_3, 1541 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VLRLR", VRS_d_3, 1541 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VLVCA", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLVCU", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLVG", VRS_b_4, 1543 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLVGP", VRR_f_3, 1543 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VLVM", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLY", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VLYD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VM", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMAD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMADS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMAE", VRR_d_5, 1569 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VMAES", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMAH", VRR_d_5, 1569 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMAL", VRR_d_5, 1568 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMALE", VRR_d_5, 1569 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMALH", VRR_d_5, 1569 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMALO", VRR_d_5, 1570 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMAO", VRR_d_5, 1570 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMCD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMCE", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMDS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VME", VRR_c_4, 1572 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VMES", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMH", VRR_c_4, 1570 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VML", VRR_c_4, 1571 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMLE", VRR_c_4, 1572 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMLH", VRR_c_4, 1571 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMLO", VRR_c_4, 1572 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMN", VRR_c_4, 1567 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMNL", VRR_c_4, 1568 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMNSD", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMNSE", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMO", VRR_c_4, 1572 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMP", VRI_f_5, 1650 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VMRH", VRR_c_4, 1544 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMRL", VRR_c_4, 1544 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMRRS", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMRSV", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMSD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMSDS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMSE", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMSES", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMSL", VRR_d_6, 1573 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VMSP", VRI_f_5, 1651 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VMX", VRR_c_4, 1567 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMXAD", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMXAE", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VMXL", VRR_c_4, 1567 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VMXSE", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VN", VRR_c_3, 1559 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VNC", VRR_c_3, 1559 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VNN", VRR_c_3, 1574 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VNO", VRR_c_3, 1574 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VNS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VNVM", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VNX", VRR_c_3, 1574 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VO", VRR_c_3, 1574 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VOC", VRR_c_3, 1575 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VOS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VOVM", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VPDI", VRR_c_4, 1547 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VPERM", VRR_e_4, 1547 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VPK", VRR_c_4, 1545 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VPKLS", VRR_b_5, 1546 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VPKS", VRR_b_5, 1545 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VPKZ", VSI_3, 1652 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VPOPCT", VRR_a_3, 1575 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VPSOP", VRI_g_5_u, 1653 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VRCL", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VREP", VRI_c_4, 1547 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VREPI", VRI_a_3, 1548 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VRP", VRI_f_5, 1654 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VRRS", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VRSV", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VRSVC", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VS", VRR_c_4, 1580 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VSBCBI", VRR_d_5, 1582 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSBI", VRR_d_5, 1581 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSCBI", VRR_c_4, 1581 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSCEF", VRV_3, 1548 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSCEG", VRV_3, 1548 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSDP", VRI_f_5, 1656 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VSDS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSE", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSEG", VRR_a_3, 1549 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSEL", VRR_e_4, 1549 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSES", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSL", VRR_c_3, 1579 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSLB", VRR_c_3, 1579 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSLD", VRI_d_4, 1607 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VSLDB", VRI_d_4, 1579 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSLL", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSP", VRI_f_5, 1658 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VSPSD", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSRA", VRR_c_3, 1579 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSRAB", VRR_c_3, 1580 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSRD", VRI_d_4, 1608 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VSRL", VRR_c_3, 1580 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VSRLB", VRR_c_3, 1580 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSRP", VRI_g_5_s, 1657 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VSRRS", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSRSV", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VST", VRX_3_opt, 1550 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VSTBR", VRX_3, 1576 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VSTD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTEB", VRX_3, 1550 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSTEBRF", VRX_3, 1576 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VSTEBRG", VRX_3, 1576 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VSTEBRH", VRX_3, 1576 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VSTEF", VRX_3, 1550 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSTEG", VRX_3, 1550 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSTEH", VRX_3, 1550 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSTER", VRX_3, 1578 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VSTH", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTI", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTID", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTK", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTKD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTL", VRS_b_3, 1552 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSTM", VRS_a_4_opt, 1551 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VSTMD", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTRC", VRR_d_6_opt, 1590 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSTRL", VSI_3, 1551 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VSTRLR", VRS_d_3, 1551 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VSTRS", VRR_d_6_opt, 1622 }, { supported_system::UNI | supported_system::SINCE_ZS9 } }, 
    { { "VSTVM", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSTVP", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VSUM", VRR_c_4, 1583 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSUMG", VRR_c_4, 1582 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSUMQ", VRR_c_4, 1583 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VSVMM", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VTM", VRR_a_2, 1584 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VTP", VRR_g_1, 1660 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VTVM", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VUPH", VRR_a_3, 1552 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VUPKZ", VSI_3, 1660 }, { supported_system::UNI | supported_system::SINCE_ZS8 } }, 
    { { "VUPL", VRR_a_3, 1553 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VUPLH", VRR_a_3, 1553 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VUPLL", VRR_a_3, 1554 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "VX", VRR_c_3, 1565 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::SINCE_ZS7 } }, 
    { { "VXELD", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VXELE", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VXS", RI_a_2_u, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VXVC", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VXVM", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VXVMM", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "VZPSD", RRE_2, 0 }, { supported_system::ESA | supported_system::XA | supported_system::_370 } }, 
    { { "WFC", VRR_a_4, 1599 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "WFK", VRR_a_4, 1600 }, { supported_system::UNI | supported_system::SINCE_ZS7 } }, 
    { { "X", RX_a_2_ux, 738 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "XC", SS_a_2_s, 739 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "XG", RXY_a_2, 738 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "XGR", RRE_2, 738 }, { supported_system::UNI | supported_system::SINCE_ZS1 } }, 
    { { "XGRK", RRF_a_3, 738 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "XI", SI_2_u, 739 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "XIHF", RIL_a_2, 740 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "XILF", RIL_a_2, 740 }, { supported_system::UNI | supported_system::SINCE_ZS3 } }, 
    { { "XIY", SIY_2_su, 739 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "XR", RR_2, 738 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
    { { "XRK", RRF_a_3, 738 }, { supported_system::UNI | supported_system::SINCE_ZS5 } }, 
    { { "XSCH", S_0, 1215 }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } }, 
    { { "XY", RXY_a_2, 738 }, { supported_system::UNI | supported_system::SINCE_ZS2 } }, 
    { { "ZAP", SS_b_2, 928 }, { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370 | supported_system::DOS | supported_system::SINCE_ZS1 } }, 
};
// clang-format on

#ifdef __cpp_lib_ranges
static_assert(std::ranges::is_sorted(machine_instructions, {}, &machine_instruction::name));

const machine_instruction* instruction::find_machine_instructions(std::string_view name)
{
    auto it = std::ranges::lower_bound(machine_instructions, name, {}, &machine_instruction::name);
    if (it == std::ranges::end(machine_instructions) || it->name() != name)
        return nullptr;
    return &*it;
}

constexpr const machine_instruction* find_mi(std::string_view name)
{
    auto it = std::ranges::lower_bound(machine_instructions, name, {}, &machine_instruction::name);
    assert(it != std::ranges::end(machine_instructions) && it->name() == name);
    return &*it;
}
#else
static_assert(std::is_sorted(std::begin(machine_instructions),
    std::end(machine_instructions),
    [](const auto& l, const auto& r) { return l.first.name() < r.first.name(); }));

const machine_instruction* instruction::find_machine_instructions(std::string_view name)
{
    auto it = std::lower_bound(
        std::begin(m_machine_instructions), std::end(m_machine_instructions), name, [](const auto& l, const auto& r) {
            return l.get().name() < r;
        });
    if (it == std::end(m_machine_instructions) || it->get().name() != name)
        return nullptr;
    return &(it->get());
}

constexpr const machine_instruction* find_mi(std::string_view name)
{
    auto it = std::lower_bound(
        std::begin(machine_instructions), std::end(machine_instructions), name, [](const auto& l, const auto& r) {
            return l.first.name() < r;
        });
    assert(it != std::end(machine_instructions) && it->first.name() == name);
    return &(it->first);
}
#endif

const machine_instruction& instruction::get_machine_instructions(std::string_view name)
{
    auto mi = find_machine_instructions(name);
    assert(mi);
    return *mi;
}

std::span<std::reference_wrapper<const machine_instruction>> instruction::all_machine_instructions()
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

constexpr std::pair<mnemonic_code, supported_system> mnemonic_codes[] = {
    { { "B", mi_BC, { { 0, 15 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BE", mi_BC, { { 0, 8 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BER", mi_BCR, { { 0, 8 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BH", mi_BC, { { 0, 2 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BHR", mi_BCR, { { 0, 2 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BI", mi_BIC, { { 0, 15 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BIE", mi_BIC, { { 0, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BIH", mi_BIC, { { 0, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BIL", mi_BIC, { { 0, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BIM", mi_BIC, { { 0, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BINE", mi_BIC, { { 0, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BINH", mi_BIC, { { 0, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BINL", mi_BIC, { { 0, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BINM", mi_BIC, { { 0, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BINO", mi_BIC, { { 0, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BINP", mi_BIC, { { 0, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BINZ", mi_BIC, { { 0, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BIO", mi_BIC, { { 0, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BIP", mi_BIC, { { 0, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BIZ", mi_BIC, { { 0, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "BL", mi_BC, { { 0, 4 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BLR", mi_BCR, { { 0, 4 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BM", mi_BC, { { 0, 4 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BMR", mi_BCR, { { 0, 4 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNE", mi_BC, { { 0, 7 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNER", mi_BCR, { { 0, 7 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNH", mi_BC, { { 0, 13 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNHR", mi_BCR, { { 0, 13 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNL", mi_BC, { { 0, 11 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNLR", mi_BCR, { { 0, 11 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNM", mi_BC, { { 0, 11 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNMR", mi_BCR, { { 0, 11 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNO", mi_BC, { { 0, 14 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNOR", mi_BCR, { { 0, 14 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNP", mi_BC, { { 0, 13 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNPR", mi_BCR, { { 0, 13 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNZ", mi_BC, { { 0, 7 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BNZR", mi_BCR, { { 0, 7 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BO", mi_BC, { { 0, 1 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BOR", mi_BCR, { { 0, 1 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BP", mi_BC, { { 0, 2 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BPR", mi_BCR, { { 0, 2 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BR", mi_BCR, { { 0, 15 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BRE", mi_BRC, { { 0, 8 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BREL", mi_BRCL, { { 0, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRH", mi_BRC, { { 0, 2 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BRHL", mi_BRCL, { { 0, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRL", mi_BRC, { { 0, 4 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BRLL", mi_BRCL, { { 0, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRM", mi_BRC, { { 0, 4 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BRML", mi_BRCL, { { 0, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRNE", mi_BRC, { { 0, 7 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BRNEL", mi_BRCL, { { 0, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRNH", mi_BRC, { { 0, 13 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BRNHL", mi_BRCL, { { 0, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRNL", mi_BRC, { { 0, 11 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BRNLL", mi_BRCL, { { 0, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRNM", mi_BRC, { { 0, 11 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BRNML", mi_BRCL, { { 0, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRNO", mi_BRC, { { 0, 14 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BRNOL", mi_BRCL, { { 0, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRNP", mi_BRC, { { 0, 13 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BRNPL", mi_BRCL, { { 0, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRNZ", mi_BRC, { { 0, 7 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BRNZL", mi_BRCL, { { 0, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRO", mi_BRC, { { 0, 1 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BROL", mi_BRCL, { { 0, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRP", mi_BRC, { { 0, 2 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BRPL", mi_BRCL, { { 0, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRU", mi_BRC, { { 0, 15 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BRUL", mi_BRCL, { { 0, 15 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BRZ", mi_BRC, { { 0, 8 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "BRZL", mi_BRCL, { { 0, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "BZ", mi_BC, { { 0, 8 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "BZR", mi_BCR, { { 0, 8 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "CGIBE", mi_CGIB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGIBH", mi_CGIB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGIBL", mi_CGIB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGIBNE", mi_CGIB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGIBNH", mi_CGIB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGIBNL", mi_CGIB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGIJE", mi_CGIJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGIJH", mi_CGIJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGIJL", mi_CGIJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGIJNE", mi_CGIJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGIJNH", mi_CGIJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGIJNL", mi_CGIJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGITE", mi_CGIT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGITH", mi_CGIT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGITL", mi_CGIT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGITNE", mi_CGIT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGITNH", mi_CGIT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGITNL", mi_CGIT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRBE", mi_CGRB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRBH", mi_CGRB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRBL", mi_CGRB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRBNE", mi_CGRB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRBNH", mi_CGRB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRBNL", mi_CGRB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRJE", mi_CGRJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRJH", mi_CGRJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRJL", mi_CGRJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRJNE", mi_CGRJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRJNH", mi_CGRJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRJNL", mi_CGRJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRTE", mi_CGRT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRTH", mi_CGRT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRTL", mi_CGRT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRTNE", mi_CGRT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRTNH", mi_CGRT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CGRTNL", mi_CGRT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CIBE", mi_CIB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CIBH", mi_CIB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CIBL", mi_CIB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CIBNE", mi_CIB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CIBNH", mi_CIB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CIBNL", mi_CIB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CIJE", mi_CIJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CIJH", mi_CIJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CIJL", mi_CIJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CIJNE", mi_CIJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CIJNH", mi_CIJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CIJNL", mi_CIJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CITE", mi_CIT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CITH", mi_CIT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CITL", mi_CIT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CITNE", mi_CIT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CITNH", mi_CIT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CITNL", mi_CIT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLFITE", mi_CLFIT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLFITH", mi_CLFIT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLFITL", mi_CLFIT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLFITNE", mi_CLFIT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLFITNH", mi_CLFIT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLFITNL", mi_CLFIT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGIBE", mi_CLGIB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGIBH", mi_CLGIB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGIBL", mi_CLGIB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGIBNE", mi_CLGIB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGIBNH", mi_CLGIB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGIBNL", mi_CLGIB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGIJE", mi_CLGIJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGIJH", mi_CLGIJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGIJL", mi_CLGIJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGIJNE", mi_CLGIJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGIJNH", mi_CLGIJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGIJNL", mi_CLGIJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGITE", mi_CLGIT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGITH", mi_CLGIT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGITL", mi_CLGIT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGITNE", mi_CLGIT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGITNH", mi_CLGIT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGITNL", mi_CLGIT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRBE", mi_CLGRB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRBH", mi_CLGRB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRBL", mi_CLGRB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRBNE", mi_CLGRB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRBNH", mi_CLGRB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRBNL", mi_CLGRB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRJE", mi_CLGRJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRJH", mi_CLGRJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRJL", mi_CLGRJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRJNE", mi_CLGRJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRJNH", mi_CLGRJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRJNL", mi_CLGRJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRTE", mi_CLGRT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRTH", mi_CLGRT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRTL", mi_CLGRT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRTNE", mi_CLGRT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRTNH", mi_CLGRT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGRTNL", mi_CLGRT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLGTE", mi_CLGT, { { 1, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS6 } },
    { { "CLGTH", mi_CLGT, { { 1, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS6 } },
    { { "CLGTL", mi_CLGT, { { 1, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS6 } },
    { { "CLGTNE", mi_CLGT, { { 1, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS6 } },
    { { "CLGTNH", mi_CLGT, { { 1, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS6 } },
    { { "CLGTNL", mi_CLGT, { { 1, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS6 } },
    { { "CLIBE", mi_CLIB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLIBH", mi_CLIB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLIBL", mi_CLIB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLIBNE", mi_CLIB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLIBNH", mi_CLIB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLIBNL", mi_CLIB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLIJE", mi_CLIJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLIJH", mi_CLIJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLIJL", mi_CLIJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLIJNE", mi_CLIJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLIJNH", mi_CLIJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLIJNL", mi_CLIJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRBE", mi_CLRB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRBH", mi_CLRB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRBL", mi_CLRB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRBNE", mi_CLRB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRBNH", mi_CLRB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRBNL", mi_CLRB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRJE", mi_CLRJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRJH", mi_CLRJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRJL", mi_CLRJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRJNE", mi_CLRJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRJNH", mi_CLRJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRJNL", mi_CLRJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRTE", mi_CLRT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRTH", mi_CLRT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRTL", mi_CLRT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRTNE", mi_CLRT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRTNH", mi_CLRT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLRTNL", mi_CLRT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CLTE", mi_CLT, { { 1, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS6 } },
    { { "CLTH", mi_CLT, { { 1, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS6 } },
    { { "CLTL", mi_CLT, { { 1, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS6 } },
    { { "CLTNE", mi_CLT, { { 1, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS6 } },
    { { "CLTNH", mi_CLT, { { 1, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS6 } },
    { { "CLTNL", mi_CLT, { { 1, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS6 } },
    { { "CRBE", mi_CRB, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRBH", mi_CRB, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRBL", mi_CRB, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRBNE", mi_CRB, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRBNH", mi_CRB, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRBNL", mi_CRB, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRJE", mi_CRJ, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRJH", mi_CRJ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRJL", mi_CRJ, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRJNE", mi_CRJ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRJNH", mi_CRJ, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRJNL", mi_CRJ, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRTE", mi_CRT, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRTH", mi_CRT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRTL", mi_CRT, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRTNE", mi_CRT, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRTNH", mi_CRT, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "CRTNL", mi_CRT, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS4 } },
    { { "J", mi_BRC, { { 0, 15 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JAS", mi_BRAS, {} }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JASL", mi_BRASL, {} }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JC", mi_BRC, {} }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JCT", mi_BRCT, {} }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JCTG", mi_BRCTG, {} }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JE", mi_BRC, { { 0, 8 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JH", mi_BRC, { { 0, 2 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JL", mi_BRC, { { 0, 4 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JLE", mi_BRCL, { { 0, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLH", mi_BRCL, { { 0, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLL", mi_BRCL, { { 0, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLM", mi_BRCL, { { 0, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLNE", mi_BRCL, { { 0, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLNH", mi_BRCL, { { 0, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLNL", mi_BRCL, { { 0, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLNM", mi_BRCL, { { 0, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLNO", mi_BRCL, { { 0, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLNOP", mi_BRCL, { { 0, 0 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JLNP", mi_BRCL, { { 0, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLNZ", mi_BRCL, { { 0, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLO", mi_BRCL, { { 0, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLP", mi_BRCL, { { 0, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLU", mi_BRCL, { { 0, 15 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JLZ", mi_BRCL, { { 0, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JM", mi_BRC, { { 0, 4 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JNE", mi_BRC, { { 0, 7 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JNH", mi_BRC, { { 0, 13 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JNL", mi_BRC, { { 0, 11 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JNM", mi_BRC, { { 0, 11 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JNO", mi_BRC, { { 0, 14 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JNOP", mi_BRC, { { 0, 0 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JNP", mi_BRC, { { 0, 13 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JNZ", mi_BRC, { { 0, 7 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JO", mi_BRC, { { 0, 1 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JP", mi_BRC, { { 0, 2 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JXH", mi_BRXH, {} }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JXHG", mi_BRXHG, {} }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JXLE", mi_BRXLE, {} }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "JXLEG", mi_BRXLG, {} }, { supported_system::UNI | supported_system::SINCE_ZS1 } },
    { { "JZ", mi_BRC, { { 0, 8 } } }, { supported_system::UNI | supported_system::ESA | supported_system::SINCE_ZS1 } },
    { { "LDRV", mi_VLLEBRZ, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "LERV", mi_VLLEBRZ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "LHHR", mi_RISBHGZ, { { 2, 0 }, { 3, 31 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LHLR", mi_RISBHGZ, { { 2, 0 }, { 3, 31 }, { 4, 32 } } },
        { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LLCHHR", mi_RISBHGZ, { { 2, 24 }, { 3, 31 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LLCHLR", mi_RISBHGZ, { { 2, 24 }, { 3, 31 }, { 4, 32 } } },
        { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LLCLHR", mi_RISBLGZ, { { 2, 24 }, { 3, 31 }, { 4, 32 } } },
        { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LLHFR", mi_RISBLGZ, { { 2, 0 }, { 3, 31 }, { 4, 32 } } },
        { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LLHHHR", mi_RISBHGZ, { { 2, 16 }, { 3, 31 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LLHHLR", mi_RISBHGZ, { { 2, 16 }, { 3, 31 }, { 4, 32 } } },
        { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LLHLHR", mi_RISBLGZ, { { 2, 16 }, { 3, 31 }, { 4, 32 } } },
        { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCE", mi_LOC, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCFHE", mi_LOCFH, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHH", mi_LOCFH, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHL", mi_LOCFH, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHNE", mi_LOCFH, { { 2, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHNH", mi_LOCFH, { { 2, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHNL", mi_LOCFH, { { 2, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHNO", mi_LOCFH, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHO", mi_LOCFH, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHRE", mi_LOCFHR, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHRH", mi_LOCFHR, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHRL", mi_LOCFHR, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHRNE", mi_LOCFHR, { { 2, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHRNH", mi_LOCFHR, { { 2, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHRNL", mi_LOCFHR, { { 2, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHRNO", mi_LOCFHR, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCFHRO", mi_LOCFHR, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCGE", mi_LOCG, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCGH", mi_LOCG, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCGHIE", mi_LOCGHI, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCGHIH", mi_LOCGHI, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCGHIL", mi_LOCGHI, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCGHINE", mi_LOCGHI, { { 2, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCGHINH", mi_LOCGHI, { { 2, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCGHINL", mi_LOCGHI, { { 2, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCGHINO", mi_LOCGHI, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCGHIO", mi_LOCGHI, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCGL", mi_LOCG, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCGNE", mi_LOCG, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCGNH", mi_LOCG, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCGNL", mi_LOCG, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCGNO", mi_LOCG, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCGO", mi_LOCG, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCGRE", mi_LOCGR, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCGRH", mi_LOCGR, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCGRL", mi_LOCGR, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCGRNE", mi_LOCGR, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCGRNH", mi_LOCGR, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCGRNL", mi_LOCGR, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCGRNO", mi_LOCGR, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCGRO", mi_LOCGR, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCH", mi_LOC, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCHHIE", mi_LOCHHI, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHHIH", mi_LOCHHI, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHHIL", mi_LOCHHI, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHHINE", mi_LOCHHI, { { 2, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHHINH", mi_LOCHHI, { { 2, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHHINL", mi_LOCHHI, { { 2, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHHINO", mi_LOCHHI, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHHIO", mi_LOCHHI, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHIE", mi_LOCHI, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHIH", mi_LOCHI, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHIL", mi_LOCHI, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHINE", mi_LOCHI, { { 2, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHINH", mi_LOCHI, { { 2, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHINL", mi_LOCHI, { { 2, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHINO", mi_LOCHI, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCHIO", mi_LOCHI, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCL", mi_LOC, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCNE", mi_LOC, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCNH", mi_LOC, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCNL", mi_LOC, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCNO", mi_LOC, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCO", mi_LOC, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCRE", mi_LOCR, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCRH", mi_LOCR, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCRL", mi_LOCR, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCRNE", mi_LOCR, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCRNH", mi_LOCR, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCRNL", mi_LOCR, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "LOCRNO", mi_LOCR, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "LOCRO", mi_LOCR, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "NHHR", mi_RNSBG, { { 2, 0 }, { 3, 31 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "NHLR", mi_RNSBG, { { 2, 0 }, { 3, 31 }, { 4, 32 } } },
        { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "NLHR", mi_RNSBG, { { 2, 32 }, { 3, 63 }, { 4, 32 } } },
        { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "NOP", mi_BC, { { 0, 0 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "NOPR", mi_BCR, { { 0, 0 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::DOS | supported_system::SINCE_ZS1 } },
    { { "NOTGR", mi_NOGRK, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "NOTR", mi_NORK, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "OHHR", mi_ROSBG, { { 2, 0 }, { 3, 31 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "OHLR", mi_ROSBG, { { 2, 0 }, { 3, 31 }, { 4, 32 } } },
        { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "OLHR", mi_ROSBG, { { 2, 32 }, { 3, 63 }, { 4, 32 } } },
        { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "SELFHRE", mi_SELFHR, { { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELFHRH", mi_SELFHR, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELFHRL", mi_SELFHR, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELFHRNE", mi_SELFHR, { { 3, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELFHRNH", mi_SELFHR, { { 3, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELFHRNL", mi_SELFHR, { { 3, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELFHRNO", mi_SELFHR, { { 3, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELFHRO", mi_SELFHR, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELGRE", mi_SELGR, { { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELGRH", mi_SELGR, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELGRL", mi_SELGR, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELGRNE", mi_SELGR, { { 3, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELGRNH", mi_SELGR, { { 3, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELGRNL", mi_SELGR, { { 3, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELGRNO", mi_SELGR, { { 3, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELGRO", mi_SELGR, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELRE", mi_SELR, { { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELRH", mi_SELR, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELRL", mi_SELR, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELRNE", mi_SELR, { { 3, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELRNH", mi_SELR, { { 3, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELRNL", mi_SELR, { { 3, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELRNO", mi_SELR, { { 3, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "SELRO", mi_SELR, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "STDRV", mi_VSTEBRG, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "STERV", mi_VSTEBRF, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "STOCE", mi_STOC, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "STOCFHE", mi_STOCFH, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "STOCFHH", mi_STOCFH, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "STOCFHL", mi_STOCFH, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "STOCFHNE", mi_STOCFH, { { 2, 7 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "STOCFHNH", mi_STOCFH, { { 2, 13 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "STOCFHNL", mi_STOCFH, { { 2, 11 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "STOCFHNO", mi_STOCFH, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "STOCFHO", mi_STOCFH, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "STOCGE", mi_STOCG, { { 2, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "STOCGH", mi_STOCG, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "STOCGL", mi_STOCG, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "STOCGNE", mi_STOCG, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "STOCGNH", mi_STOCG, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "STOCGNL", mi_STOCG, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "STOCGNO", mi_STOCG, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "STOCGO", mi_STOCG, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "STOCH", mi_STOC, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "STOCL", mi_STOC, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "STOCNE", mi_STOC, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "STOCNH", mi_STOC, { { 2, 12 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "STOCNL", mi_STOC, { { 2, 10 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "STOCNO", mi_STOC, { { 2, 14 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "STOCO", mi_STOC, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VAB", mi_VA, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VACCB", mi_VACC, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VACCCQ", mi_VACCC, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VACCF", mi_VACC, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VACCG", mi_VACC, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VACCH", mi_VACC, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VACCQ", mi_VACC, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VACQ", mi_VAC, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VAF", mi_VA, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VAG", mi_VA, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VAH", mi_VA, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VAQ", mi_VA, { { 3, 4 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::SINCE_ZS7 } },
    { { "VAVGB", mi_VAVG, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VAVGF", mi_VAVG, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VAVGG", mi_VAVG, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VAVGH", mi_VAVG, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VAVGLB", mi_VAVGL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VAVGLF", mi_VAVGL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VAVGLG", mi_VAVGL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VAVGLH", mi_VAVGL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCDG", mi_VCFPS, {} }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCDGB", mi_VCFPS, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCDLG", mi_VCFPL, {} }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCDLGB", mi_VCFPL, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCEFB", mi_VCFPS, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VCELFB", mi_VCFPL, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VCEQB", mi_VCEQ, { { 3, 0 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCEQBS", mi_VCEQ, { { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCEQF", mi_VCEQ, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCEQFS", mi_VCEQ, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCEQG", mi_VCEQ, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCEQGS", mi_VCEQ, { { 3, 3 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCEQH", mi_VCEQ, { { 3, 1 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCEQHS", mi_VCEQ, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCFEB", mi_VCSFP, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VCGD", mi_VCSFP, {} }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCGDB", mi_VCSFP, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHB", mi_VCH, { { 3, 0 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHBS", mi_VCH, { { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHF", mi_VCH, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHFS", mi_VCH, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHG", mi_VCH, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHGS", mi_VCH, { { 3, 3 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHH", mi_VCH, { { 3, 1 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHHS", mi_VCH, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHLB", mi_VCHL, { { 3, 0 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHLBS", mi_VCHL, { { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHLF", mi_VCHL, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHLFS", mi_VCHL, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHLG", mi_VCHL, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHLGS", mi_VCHL, { { 3, 3 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHLH", mi_VCHL, { { 3, 1 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCHLHS", mi_VCHL, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCLFEB", mi_VCLFP, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VCLGDB", mi_VCLGD, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCLZB", mi_VCLZ, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCLZF", mi_VCLZ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCLZG", mi_VCLZ, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VCLZH", mi_VCLZ, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VECB", mi_VEC, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VECF", mi_VEC, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VECG", mi_VEC, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VECH", mi_VEC, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VECLB", mi_VECL, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VECLF", mi_VECL, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VECLG", mi_VECL, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VECLH", mi_VECL, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VERIMB", mi_VERIM, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VERIMF", mi_VERIM, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VERIMG", mi_VERIM, { { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VERIMH", mi_VERIM, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VERLLB", mi_VERLL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VERLLF", mi_VERLL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VERLLG", mi_VERLL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VERLLH", mi_VERLL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VERLLVB", mi_VERLLV, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VERLLVF", mi_VERLLV, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VERLLVG", mi_VERLLV, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VERLLVH", mi_VERLLV, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESLB", mi_VESL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESLF", mi_VESL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESLG", mi_VESL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESLH", mi_VESL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESLVB", mi_VESLV, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESLVF", mi_VESLV, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESLVG", mi_VESLV, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESLVH", mi_VESLV, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRAB", mi_VESRA, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRAF", mi_VESRA, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRAG", mi_VESRA, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRAH", mi_VESRA, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRAVB", mi_VESRAV, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRAVF", mi_VESRAV, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRAVG", mi_VESRAV, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRAVH", mi_VESRAV, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRLB", mi_VESRL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRLF", mi_VESRL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRLG", mi_VESRL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRLH", mi_VESRL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRLVB", mi_VESRLV, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRLVF", mi_VESRLV, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRLVG", mi_VESRLV, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VESRLVH", mi_VESRLV, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFADB", mi_VFA, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFAEB", mi_VFAE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFAEBS", mi_VFAE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFAEF", mi_VFAE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFAEFS", mi_VFAE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFAEH", mi_VFAE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFAEHS", mi_VFAE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFAEZB", mi_VFAE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFAEZBS", mi_VFAE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFAEZF", mi_VFAE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFAEZFS", mi_VFAE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFAEZH", mi_VFAE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFAEZHS", mi_VFAE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFASB", mi_VFA, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFCEDB", mi_VFCE, { { 3, 3 }, { 4, 0 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFCEDBS", mi_VFCE, { { 3, 3 }, { 4, 0 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFCESB", mi_VFCE, { { 3, 2 }, { 4, 0 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFCESBS", mi_VFCE, { { 3, 2 }, { 4, 0 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFCHDB", mi_VFCH, { { 3, 3 }, { 4, 0 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFCHDBS", mi_VFCH, { { 3, 3 }, { 4, 0 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFCHEDB", mi_VFCHE, { { 3, 3 }, { 4, 0 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFCHEDBS", mi_VFCHE, { { 3, 3 }, { 4, 0 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFCHESB", mi_VFCHE, { { 3, 2 }, { 4, 0 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFCHESBS", mi_VFCHE, { { 3, 2 }, { 4, 0 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFCHSB", mi_VFCH, { { 3, 2 }, { 4, 0 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFCHSBS", mi_VFCH, { { 3, 2 }, { 4, 0 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFDDB", mi_VFD, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFDSB", mi_VFD, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFEEB", mi_VFEE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFEEBS", mi_VFEE, { { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFEEF", mi_VFEE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFEEFS", mi_VFEE, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFEEGS", mi_VFEE, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNKNOWN } },
    { { "VFEEH", mi_VFEE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFEEZB", mi_VFEE, { { 3, 0 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFEEZBS", mi_VFEE, { { 3, 0 }, { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFEEZF", mi_VFEE, { { 3, 2 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFEEZFS", mi_VFEE, { { 3, 2 }, { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFEEZH", mi_VFEE, { { 3, 1 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFEEZHS", mi_VFEE, { { 3, 1 }, { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFENEB", mi_VFENE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFENEBS", mi_VFENE, { { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFENEF", mi_VFENE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFENEFS", mi_VFENE, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFENEH", mi_VFENE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFENEHS", mi_VFENE, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFENEZB", mi_VFENE, { { 3, 0 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFENEZBS", mi_VFENE, { { 3, 0 }, { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFENEZF", mi_VFENE, { { 3, 2 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFENEZFS", mi_VFENE, { { 3, 2 }, { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFENEZH", mi_VFENE, { { 3, 1 }, { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFENEZHS", mi_VFENE, { { 3, 1 }, { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFIDB", mi_VFI, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFISB", mi_VFI, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFKEDB", mi_VFCE, { { 3, 3 }, { 4, 4 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFKEDBS", mi_VFCE, { { 3, 3 }, { 4, 4 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFKESB", mi_VFCE, { { 3, 2 }, { 4, 4 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFKESBS", mi_VFCE, { { 3, 2 }, { 4, 4 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFKHDB", mi_VFCH, { { 3, 3 }, { 4, 4 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFKHDBS", mi_VFCH, { { 3, 3 }, { 4, 4 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFKHEDB", mi_VFCHE, { { 3, 3 }, { 4, 4 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFKHEDBS", mi_VFCHE, { { 3, 3 }, { 4, 4 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFKHESB", mi_VFCHE, { { 3, 2 }, { 4, 4 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFKHESBS", mi_VFCHE, { { 3, 2 }, { 4, 4 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFKHSB", mi_VFCH, { { 3, 2 }, { 4, 4 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFKHSBS", mi_VFCH, { { 3, 2 }, { 4, 4 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFLCDB", mi_VFPSO, { { 2, 3 }, { 3, 0 }, { 4, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFLCSB", mi_VFPSO, { { 2, 2 }, { 3, 0 }, { 4, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFLLS", mi_VFLL, { { 2, 2 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFLNDB", mi_VFPSO, { { 2, 3 }, { 3, 0 }, { 4, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFLNSB", mi_VFPSO, { { 2, 2 }, { 3, 0 }, { 4, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFLPDB", mi_VFPSO, { { 2, 3 }, { 3, 0 }, { 4, 2 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFLPSB", mi_VFPSO, { { 2, 2 }, { 3, 0 }, { 4, 2 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFLRD", mi_VFLR, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFMADB", mi_VFMA, { { 4, 0 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFMASB", mi_VFMA, { { 4, 0 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFMAXDB", mi_VFMAX, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFMAXSB", mi_VFMAX, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFMDB", mi_VFM, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFMINDB", mi_VFMIN, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFMINSB", mi_VFMIN, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFMSB", mi_VFM, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFMSDB", mi_VFMS, { { 4, 0 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFMSSB", mi_VFMS, { { 4, 0 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFNMADB", mi_VFNMA, { { 4, 0 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFNMASB", mi_VFNMA, { { 4, 0 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFNMSDB", mi_VFNMS, { { 4, 0 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFNMSSB", mi_VFNMS, { { 4, 0 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFPSODB", mi_VFPSO, { { 2, 3 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFPSOSB", mi_VFPSO, { { 2, 2 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFSDB", mi_VFS, { { 2, 3 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFSQDB", mi_VFSQ, { { 2, 3 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFSQSB", mi_VFSQ, { { 2, 2 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFSSB", mi_VFS, { { 2, 2 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VFTCIDB", mi_VFTCI, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VFTCISB", mi_VFTCI, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VGFMAB", mi_VGFMA, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VGFMAF", mi_VGFMA, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VGFMAG", mi_VGFMA, { { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VGFMAH", mi_VGFMA, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VGFMB", mi_VGFM, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VGFMF", mi_VGFM, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VGFMG", mi_VGFM, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VGFMH", mi_VGFM, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VGMB", mi_VGM, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VGMF", mi_VGM, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VGMG", mi_VGM, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VGMH", mi_VGM, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VISTRB", mi_VISTR, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VISTRBS", mi_VISTR, { { 3, 0 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VISTRF", mi_VISTR, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VISTRFS", mi_VISTR, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VISTRH", mi_VISTR, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VISTRHS", mi_VISTR, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLBRF", mi_VLBR, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLBRG", mi_VLBR, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLBRH", mi_VLBR, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLBRQ", mi_VLBR, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLBRREPF", mi_VLBRREP, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLBRREPG", mi_VLBRREP, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLBRREPH", mi_VLBRREP, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLCB", mi_VLC, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLCF", mi_VLC, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLCG", mi_VLC, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLCH", mi_VLC, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLDE", mi_VFLL, {} }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLDEB", mi_VFLL, { { 2, 2 }, { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLED", mi_VFLR, {} }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLEDB", mi_VFLR, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLERF", mi_VLER, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLERG", mi_VLER, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLERH", mi_VLER, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLGVB", mi_VLGV, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLGVF", mi_VLGV, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLGVG", mi_VLGV, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLGVH", mi_VLGV, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLLEBRZE", mi_VLLEBRZ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLLEBRZF", mi_VLLEBRZ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLLEBRZG", mi_VLLEBRZ, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLLEBRZH", mi_VLLEBRZ, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VLLEZB", mi_VLLEZ, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLLEZF", mi_VLLEZ, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLLEZG", mi_VLLEZ, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLLEZH", mi_VLLEZ, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLLEZLF", mi_VLLEZ, { { 2, 6 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VLPB", mi_VLP, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLPF", mi_VLP, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLPG", mi_VLP, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLPH", mi_VLP, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLREPB", mi_VLREP, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLREPF", mi_VLREP, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLREPG", mi_VLREP, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLREPH", mi_VLREP, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLVGB", mi_VLVG, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLVGF", mi_VLVG, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLVGG", mi_VLVG, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VLVGH", mi_VLVG, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMAEB", mi_VMAE, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMAEF", mi_VMAE, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMAEH", mi_VMAE, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMAHB", mi_VMAH, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMAHF", mi_VMAH, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMAHH", mi_VMAH, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMALB", mi_VMAL, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMALEB", mi_VMALE, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMALEF", mi_VMALE, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMALEH", mi_VMALE, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMALF", mi_VMAL, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMALHB", mi_VMALH, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMALHF", mi_VMALH, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMALHH", mi_VMALH, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMALHW", mi_VMAL, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMALOB", mi_VMALO, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMALOF", mi_VMALO, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMALOH", mi_VMALO, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMAOB", mi_VMAO, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMAOF", mi_VMAO, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMAOH", mi_VMAO, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMEB", mi_VME, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMEF", mi_VME, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMEH", mi_VME, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMHB", mi_VMH, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMHF", mi_VMH, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMHH", mi_VMH, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMLB", mi_VML, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMLEB", mi_VMLE, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMLEF", mi_VMLE, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMLEH", mi_VMLE, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMLF", mi_VML, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMLHB", mi_VMLH, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMLHF", mi_VMLH, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMLHH", mi_VMLH, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMLHW", mi_VML, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMLOB", mi_VMLO, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMLOF", mi_VMLO, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMLOH", mi_VMLO, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMNB", mi_VMN, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMNF", mi_VMN, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMNG", mi_VMN, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMNH", mi_VMN, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMNLB", mi_VMNL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMNLF", mi_VMNL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMNLG", mi_VMNL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMNLH", mi_VMNL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMOB", mi_VMO, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMOF", mi_VMO, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMOH", mi_VMO, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMRHB", mi_VMRH, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMRHF", mi_VMRH, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMRHG", mi_VMRH, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMRHH", mi_VMRH, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMRLB", mi_VMRL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMRLF", mi_VMRL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMRLG", mi_VMRL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMRLH", mi_VMRL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMSLG", mi_VMSL, { { 4, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VMXB", mi_VMX, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMXF", mi_VMX, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMXG", mi_VMX, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMXH", mi_VMX, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMXLB", mi_VMXL, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMXLF", mi_VMXL, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMXLG", mi_VMXL, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VMXLH", mi_VMXL, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VNOT", mi_VNO, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VONE", mi_VGBM, { { 1, 65535 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKF", mi_VPK, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKG", mi_VPK, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKH", mi_VPK, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKLSF", mi_VPKLS, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKLSFS", mi_VPKLS, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKLSG", mi_VPKLS, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKLSGS", mi_VPKLS, { { 3, 3 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKLSH", mi_VPKLS, { { 3, 1 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKLSHS", mi_VPKLS, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKSF", mi_VPKS, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKSFS", mi_VPKS, { { 3, 2 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKSG", mi_VPKS, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKSGS", mi_VPKS, { { 3, 3 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKSH", mi_VPKS, { { 3, 1 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPKSHS", mi_VPKS, { { 3, 1 }, { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VPOPCTB", mi_VPOPCT, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VPOPCTF", mi_VPOPCT, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VPOPCTG", mi_VPOPCT, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VPOPCTH", mi_VPOPCT, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "VREPB", mi_VREP, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VREPF", mi_VREP, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VREPG", mi_VREP, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VREPH", mi_VREP, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VREPIB", mi_VREPI, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VREPIF", mi_VREPI, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VREPIG", mi_VREPI, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VREPIH", mi_VREPI, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSB", mi_VS, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSBCBIQ", mi_VSBCBI, { { 4, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSBIQ", mi_VSBI, { { 4, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSCBIB", mi_VSCBI, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSCBIF", mi_VSCBI, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSCBIG", mi_VSCBI, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSCBIH", mi_VSCBI, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSCBIQ", mi_VSCBI, { { 3, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSEGB", mi_VSEG, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSEGF", mi_VSEG, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSEGH", mi_VSEG, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSF", mi_VS, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSG", mi_VS, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSH", mi_VS, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSQ", mi_VS, { { 3, 4 } } },
        { supported_system::UNI | supported_system::ESA | supported_system::XA | supported_system::_370
            | supported_system::SINCE_ZS7 } },
    { { "VSTBRF", mi_VSTBR, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VSTBRG", mi_VSTBR, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VSTBRH", mi_VSTBR, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VSTBRQ", mi_VSTBR, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VSTERF", mi_VSTER, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VSTERG", mi_VSTER, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VSTERH", mi_VSTER, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VSTRCB", mi_VSTRC, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSTRCBS", mi_VSTRC, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSTRCF", mi_VSTRC, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSTRCFS", mi_VSTRC, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSTRCH", mi_VSTRC, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSTRCHS", mi_VSTRC, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSTRCZB", mi_VSTRC, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSTRCZBS", mi_VSTRC, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSTRCZF", mi_VSTRC, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSTRCZFS", mi_VSTRC, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSTRCZH", mi_VSTRC, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSTRCZHS", mi_VSTRC, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSTRSB", mi_VSTRS, { { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VSTRSF", mi_VSTRS, { { 4, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VSTRSH", mi_VSTRS, { { 4, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VSTRSZB", mi_VSTRS, { { 4, 0 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "VSUMB", mi_VSUM, { { 3, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSUMGF", mi_VSUMG, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSUMGH", mi_VSUMG, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSUMH", mi_VSUM, { { 3, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSUMQF", mi_VSUMQ, { { 3, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VSUMQG", mi_VSUMQ, { { 3, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VUPHB", mi_VUPH, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VUPHF", mi_VUPH, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VUPHH", mi_VUPH, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VUPLB", mi_VUPL, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VUPLF", mi_VUPL, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VUPLHB", mi_VUPLH, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VUPLHF", mi_VUPLH, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VUPLHG", mi_VUPLH, { { 2, 1 } } }, { supported_system::UNKNOWN } },
    { { "VUPLHW", mi_VUPL, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VUPLLB", mi_VUPLL, { { 2, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VUPLLF", mi_VUPLL, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VUPLLH", mi_VUPLL, { { 2, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "VZERO", mi_VGBM, { { 0, 1 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WCDGB", mi_VCFPS, { { 2, 3 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } }, // operand with index 3 ORed with 8
    { { "WCDLGB", mi_VCFPL, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WCEFB", mi_VCFPS, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "WCELFB", mi_VCFPL, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "WCFEB", mi_VCSFP, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "WCGDB", mi_VCSFP, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WCLFEB", mi_VCLFP, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS9 } },
    { { "WCLGDB", mi_VCLGD, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFADB", mi_VFA, { { 3, 3 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFASB", mi_VFA, { { 3, 2 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFAXB", mi_VFA, { { 3, 4 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCDB", mi_WFC, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFCEDB", mi_VFCE, { { 3, 3 }, { 4, 8 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFCEDBS", mi_VFCE, { { 3, 3 }, { 4, 8 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFCESB", mi_VFCE, { { 3, 2 }, { 4, 8 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCESBS", mi_VFCE, { { 3, 2 }, { 4, 8 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCEXB", mi_VFCE, { { 3, 4 }, { 4, 8 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCEXBS", mi_VFCE, { { 3, 4 }, { 4, 8 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCHDB", mi_VFCH, { { 3, 3 }, { 4, 8 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFCHDBS", mi_VFCH, { { 3, 3 }, { 4, 8 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFCHEDB", mi_VFCHE, { { 3, 3 }, { 4, 8 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFCHEDBS", mi_VFCHE, { { 3, 3 }, { 4, 8 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFCHESB", mi_VFCHE, { { 3, 2 }, { 4, 8 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCHESBS", mi_VFCHE, { { 3, 2 }, { 4, 8 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCHEXB", mi_VFCHE, { { 3, 4 }, { 4, 8 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCHEXBS", mi_VFCHE, { { 3, 4 }, { 4, 8 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCHSB", mi_VFCH, { { 3, 2 }, { 4, 8 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCHSBS", mi_VFCH, { { 3, 2 }, { 4, 8 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCHXB", mi_VFCH, { { 3, 4 }, { 4, 8 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCHXBS", mi_VFCH, { { 3, 4 }, { 4, 8 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCSB", mi_WFC, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFCXB", mi_WFC, { { 3, 4 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFDDB", mi_VFD, { { 3, 3 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFDSB", mi_VFD, { { 3, 2 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFDXB", mi_VFD, { { 3, 4 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFIDB", mi_VFI, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFISB", mi_VFI, { { 2, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFIXB", mi_VFI, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKDB", mi_WFK, { { 3, 3 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFKEDB", mi_VFCE, { { 3, 3 }, { 4, 12 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKEDBS", mi_VFCE, { { 3, 3 }, { 4, 12 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKESB", mi_VFCE, { { 3, 2 }, { 4, 12 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKESBS", mi_VFCE, { { 3, 2 }, { 4, 12 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKEXB", mi_VFCE, { { 3, 4 }, { 4, 12 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKEXBS", mi_VFCE, { { 3, 4 }, { 4, 12 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKHDB", mi_VFCH, { { 3, 3 }, { 4, 12 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKHDBS", mi_VFCH, { { 3, 3 }, { 4, 12 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKHEDB", mi_VFCHE, { { 3, 3 }, { 4, 12 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKHEDBS", mi_VFCHE, { { 3, 3 }, { 4, 12 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKHESB", mi_VFCHE, { { 3, 2 }, { 4, 12 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKHESBS", mi_VFCHE, { { 3, 2 }, { 4, 12 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKHEXB", mi_VFCHE, { { 3, 4 }, { 4, 12 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKHEXBS", mi_VFCHE, { { 3, 4 }, { 4, 12 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKHSB", mi_VFCH, { { 3, 2 }, { 4, 12 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKHSBS", mi_VFCH, { { 3, 2 }, { 4, 12 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKHXB", mi_VFCH, { { 3, 4 }, { 4, 12 }, { 5, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKHXBS", mi_VFCH, { { 3, 4 }, { 4, 12 }, { 5, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKSB", mi_WFK, { { 3, 2 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFKXB", mi_WFK, { { 3, 4 }, { 4, 0 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFLCDB", mi_VFPSO, { { 2, 3 }, { 3, 8 }, { 4, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFLCSB", mi_VFPSO, { { 2, 2 }, { 3, 8 }, { 4, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFLCXB", mi_VFPSO, { { 2, 4 }, { 3, 8 }, { 4, 0 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFLLD", mi_VFLL, { { 2, 3 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFLLS", mi_VFLL, { { 2, 2 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFLNDB", mi_VFPSO, { { 2, 3 }, { 3, 8 }, { 4, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFLNSB", mi_VFPSO, { { 2, 2 }, { 3, 8 }, { 4, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFLNXB", mi_VFPSO, { { 2, 4 }, { 3, 8 }, { 4, 1 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFLPDB", mi_VFPSO, { { 2, 3 }, { 3, 8 }, { 4, 2 } } },
        { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFLPSB", mi_VFPSO, { { 2, 2 }, { 3, 8 }, { 4, 2 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFLPXB", mi_VFPSO, { { 2, 4 }, { 3, 8 }, { 4, 2 } } },
        { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFLRD", mi_VFLR, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFLRX", mi_VFLR, { { 2, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFMADB", mi_VFMA, { { 4, 8 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFMASB", mi_VFMA, { { 4, 8 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFMAXB", mi_VFMA, { { 4, 8 }, { 5, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFMAXDB", mi_VFMAX, { { 3, 3 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFMAXSB", mi_VFMAX, { { 3, 2 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFMAXXB", mi_VFMAX, { { 3, 4 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFMDB", mi_VFM, { { 3, 3 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFMINDB", mi_VFMIN, { { 3, 3 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFMINSB", mi_VFMIN, { { 3, 2 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFMINXB", mi_VFMIN, { { 3, 4 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFMSB", mi_VFM, { { 3, 2 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFMSDB", mi_VFMS, { { 4, 8 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFMSSB", mi_VFMS, { { 4, 8 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFMSXB", mi_VFMS, { { 4, 8 }, { 5, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFMXB", mi_VFM, { { 3, 4 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFNMADB", mi_VFNMA, { { 4, 8 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFNMASB", mi_VFNMA, { { 4, 8 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFNMAXB", mi_VFNMA, { { 4, 8 }, { 5, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFNMSDB", mi_VFNMS, { { 4, 8 }, { 5, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFNMSSB", mi_VFNMS, { { 4, 8 }, { 5, 2 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFNMSXB", mi_VFNMS, { { 4, 8 }, { 5, 4 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFPSODB", mi_VFPSO, { { 2, 3 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFPSOSB", mi_VFPSO, { { 2, 2 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFPSOXB", mi_VFPSO, { { 2, 4 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFSDB", mi_VFS, { { 2, 3 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFSQDB", mi_VFSQ, { { 2, 3 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFSQSB", mi_VFSQ, { { 2, 2 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFSQXB", mi_VFSQ, { { 2, 4 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFSSB", mi_VFS, { { 2, 2 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFSXB", mi_VFS, { { 2, 4 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFTCIDB", mi_VFTCI, { { 3, 3 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WFTCISB", mi_VFTCI, { { 3, 2 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WFTCIXB", mi_VFTCI, { { 3, 4 }, { 4, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS8 } },
    { { "WLDEB", mi_VFLL, { { 2, 2 }, { 3, 8 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "WLEDB", mi_VFLR, { { 2, 3 } } }, { supported_system::UNI | supported_system::SINCE_ZS7 } },
    { { "XHHR", mi_RXSBG, { { 2, 0 }, { 3, 31 } } }, { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "XHLR", mi_RXSBG, { { 2, 0 }, { 3, 31 }, { 4, 32 } } },
        { supported_system::UNI | supported_system::SINCE_ZS5 } },
    { { "XLHR", mi_RXSBG, { { 2, 32 }, { 3, 63 }, { 4, 32 } } },
        { supported_system::UNI | supported_system::SINCE_ZS5 } },
};

#ifdef __cpp_lib_ranges
static_assert(
    std::ranges::is_sorted(mnemonic_codes, {}, [](const auto& mnemonic) { return mnemonic.first.name; })); // todo

const mnemonic_code* instruction::find_mnemonic_codes(std::string_view name)
{
    auto it = std::ranges::lower_bound(m_mnemonic_codes, name, {}, &mnemonic_code::name);
    if (it == std::ranges::end(m_mnemonic_codes) || it->name() != name)
        return nullptr;
    return &*it;
}
#else
static_assert(std::is_sorted(std::begin(mnemonic_codes), std::end(mnemonic_codes), [](const auto& l, const auto& r) {
    return l.first.name() < r.first.name();
}));

const mnemonic_code* instruction::find_mnemonic_codes(std::string_view name)
{
    auto it = std::lower_bound(
        std::begin(m_mnemonic_codes), std::end(m_mnemonic_codes), name, [](const auto& l, const auto& r) {
            return l.get().name() < r;
        });
    if (it == std::end(m_mnemonic_codes) || it->get().name() != name)
        return nullptr;
    return &(it->get());
}
#endif

const mnemonic_code& instruction::get_mnemonic_codes(std::string_view name)
{
    auto result = find_mnemonic_codes(name);
    assert(result);
    return *result;
}
std::span<std::reference_wrapper<const mnemonic_code>> instruction::all_mnemonic_codes() { return m_mnemonic_codes; }

std::vector<std::reference_wrapper<const machine_instruction>> instruction::m_machine_instructions = {};
std::vector<std::reference_wrapper<const mnemonic_code>> instruction::m_mnemonic_codes = {};

instruction::instruction(system_architecture arch)
    : m_arch(arch)
{
    m_mnemonic_codes.clear();
    for (const auto& mnemonic : mnemonic_codes)
    {
        if (is_instruction_supported(mnemonic.second))
        {
            m_mnemonic_codes.push_back(mnemonic.first);
        }
    }

    m_machine_instructions.clear();
    for (const auto& mi : machine_instructions)
    {
        if (is_instruction_supported(mi.second))
        {
            m_machine_instructions.push_back(mi.first);
        }
    }
}

bool instruction::is_instruction_supported(supported_system instruction_support)
{
    if ((instruction_support & supported_system::UNKNOWN) == supported_system::UNKNOWN)
    {
        return true;
    }

    switch (m_arch)
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
        case system_architecture::ZS1:
        case system_architecture::ZS2:
        case system_architecture::ZS3:
        case system_architecture::ZS4:
        case system_architecture::ZS5:
        case system_architecture::ZS6:
        case system_architecture::ZS7:
        case system_architecture::ZS8:
        case system_architecture::ZS9: {
            size_t zs_arch_mask = 0x0F;
            instruction_support = instruction_support & zs_arch_mask;

            return instruction_support == supported_system::NO_ZS_SUPPORT ? false : instruction_support <= m_arch;
        }
        // case system_architecture::ZS1: {
        //     return (instruction_support & supported_system::SINCE_ZS1) == supported_system::SINCE_ZS1;
        // }
        // case system_architecture::ZS2: {
        //     return (instruction_support & supported_system::SINCE_ZS2) == supported_system::SINCE_ZS2;
        // }
        // case system_architecture::ZS3: {
        //     return (instruction_support & supported_system::SINCE_ZS3) == supported_system::SINCE_ZS3;
        // }
        // case system_architecture::ZS4: {
        //     return (instruction_support & supported_system::SINCE_ZS4) == supported_system::SINCE_ZS4;
        // }
        // case system_architecture::ZS5: {
        //     return (instruction_support & supported_system::SINCE_ZS5) == supported_system::SINCE_ZS5;
        // }
        // case system_architecture::ZS6: {
        //     return (instruction_support & supported_system::SINCE_ZS6) == supported_system::SINCE_ZS6;
        // }
        // case system_architecture::ZS7: {
        //     return (instruction_support & supported_system::SINCE_ZS7) == supported_system::SINCE_ZS7;
        // }
        // case system_architecture::ZS8: {
        //     return (instruction_support & supported_system::SINCE_ZS8) == supported_system::SINCE_ZS8;
        // }
        // case system_architecture::ZS9: {
        //     return (instruction_support & supported_system::SINCE_ZS9) == supported_system::SINCE_ZS9;
        // }
        default:
            return false;
    }
}
