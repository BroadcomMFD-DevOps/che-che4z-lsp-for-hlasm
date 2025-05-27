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
#include <array>
#include <cassert>
#include <memory>
#include <numeric>
#include <utility>

#include "checking/diagnostic_collector.h"
#include "checking/instr_operand.h"

namespace hlasm_plugin::parser_library::context {

namespace {

consteval instruction_set_affiliation operator|(instruction_set_affiliation a, z_arch_affiliation z_affil) noexcept
{
    using enum z_arch_affiliation;
    assert(a.z_arch == NO_AFFILIATION && a.z_arch_removed == NO_AFFILIATION);

    a.z_arch = z_affil;
    a.z_arch_removed = LAST;

    return a;
}

consteval instruction_set_affiliation operator|(instruction_set_affiliation a, instruction_set_affiliation b) noexcept
{
    assert(a.z_arch == z_arch_affiliation::NO_AFFILIATION);
    assert(b.z_arch == z_arch_affiliation::NO_AFFILIATION);

    instruction_set_affiliation result {};

    result.z_arch = z_arch_affiliation::NO_AFFILIATION;
    result.esa = a.esa | b.esa;
    result.xa = a.xa | b.xa;
    result._370 = a._370 | b._370;
    result.dos = a.dos | b.dos;
    result.uni = a.uni | b.uni;

    return result;
}

// clang-format off
constexpr auto ESA       = instruction_set_affiliation{z_arch_affiliation::NO_AFFILIATION, z_arch_affiliation::NO_AFFILIATION, 1, 0, 0, 0, 0};
constexpr auto XA        = instruction_set_affiliation{z_arch_affiliation::NO_AFFILIATION, z_arch_affiliation::NO_AFFILIATION, 0, 1, 0, 0, 0};
constexpr auto _370      = instruction_set_affiliation{z_arch_affiliation::NO_AFFILIATION, z_arch_affiliation::NO_AFFILIATION, 0, 0, 1, 0, 0};
constexpr auto DOS       = instruction_set_affiliation{z_arch_affiliation::NO_AFFILIATION, z_arch_affiliation::NO_AFFILIATION, 0, 0, 0, 1, 0};
constexpr auto UNI       = instruction_set_affiliation{z_arch_affiliation::NO_AFFILIATION, z_arch_affiliation::NO_AFFILIATION, 0, 0, 0, 0, 1};

constexpr auto ESA_XA                       = ESA | XA;
constexpr auto ESA_XA_370                   = ESA | XA | _370;
constexpr auto UNI_370                      = UNI | _370;
constexpr auto UNI_370_DOS                  = UNI | _370 | DOS;
constexpr auto UNI_ESA_SINCE_ZOP            = UNI | ESA | z_arch_affiliation::ZOP;
constexpr auto UNI_ESA_XA_370_DOS_SINCE_ZOP = UNI | ESA | XA | _370 | DOS | z_arch_affiliation::ZOP;
constexpr auto UNI_ESA_XA_370_SINCE_Z13     = UNI | ESA | XA | _370 | z_arch_affiliation::Z13;
constexpr auto UNI_ESA_XA_370_SINCE_Z15     = UNI | ESA | XA | _370 | z_arch_affiliation::Z15;
constexpr auto UNI_ESA_XA_370_SINCE_ZOP     = UNI | ESA | XA | _370 | z_arch_affiliation::ZOP;
constexpr auto UNI_ESA_XA_SINCE_ZOP         = UNI | ESA | XA | z_arch_affiliation::ZOP;
constexpr auto UNI_SINCE_YOP                = UNI | z_arch_affiliation::YOP;
constexpr auto UNI_SINCE_Z10                = UNI | z_arch_affiliation::Z10;
constexpr auto UNI_SINCE_Z11                = UNI | z_arch_affiliation::Z11;
constexpr auto UNI_SINCE_Z12                = UNI | z_arch_affiliation::Z12;
constexpr auto UNI_SINCE_Z13                = UNI | z_arch_affiliation::Z13;
constexpr auto UNI_SINCE_Z14                = UNI | z_arch_affiliation::Z14;
constexpr auto UNI_SINCE_Z15                = UNI | z_arch_affiliation::Z15;
constexpr auto UNI_SINCE_Z16                = UNI | z_arch_affiliation::Z16;
constexpr auto UNI_SINCE_Z17                = UNI | z_arch_affiliation::Z17;
constexpr auto UNI_SINCE_Z9                 = UNI | z_arch_affiliation::Z9;
constexpr auto UNI_SINCE_ZOP                = UNI | z_arch_affiliation::ZOP;
constexpr auto SINCE_YOP_TILL_Z17           = instruction_set_affiliation{z_arch_affiliation::YOP, z_arch_affiliation::Z17, 0, 0, 0, 0, 0};
constexpr auto SINCE_Z14_TILL_Z17           = instruction_set_affiliation{z_arch_affiliation::Z14, z_arch_affiliation::Z17, 0, 0, 0, 0, 0};
// clang-format on
} // namespace

std::string_view instruction::mach_format_to_string(mach_format f) noexcept
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
        case mach_format::RXY_c:
            return "RXY-c";
        case mach_format::S:
            return "S";
        case mach_format::SI:
            return "SI";
        case mach_format::DIAGNOSE:
            return "DIAGNOSE";
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
        case mach_format::VRI_j:
            return "VRI-j";
        case mach_format::VRI_k:
            return "VRI-k";
        case mach_format::VRI_l:
            return "VRI-l";
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
        case mach_format::VRR_j:
            return "VRR-j";
        case mach_format::VRR_k:
            return "VRR-k";
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
    }
    assert(false);
    return "";
}

template<unsigned char n>
consteval inline_string<n>::inline_string(std::string_view s) noexcept
    : len((unsigned char)s.size())
    , data {}
{
    assert(s.size() <= n);
    size_t i = 0;
    for (char c : s)
        data[i++] = c;
}

consteval ca_instruction::ca_instruction(std::string_view n, bool opless) noexcept
    : m_name(n)
    , m_operandless(opless)
{}

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

static_assert(std::ranges::is_sorted(ca_instructions, {}, &ca_instruction::name));

const ca_instruction* instruction::find_ca_instructions(std::string_view name) noexcept
{
    auto it = std::ranges::lower_bound(ca_instructions, name, {}, &ca_instruction::name);

    if (it == std::ranges::end(ca_instructions) || it->name() != name)
        return nullptr;
    return std::to_address(it);
}

const ca_instruction& instruction::get_ca_instructions(std::string_view name) noexcept
{
    auto result = find_ca_instructions(name);
    assert(result);
    return *result;
}

std::span<const ca_instruction> instruction::all_ca_instructions() noexcept { return ca_instructions; }

consteval assembler_instruction::assembler_instruction(std::string_view name,
    int min_operands,
    int max_operands,
    bool has_ord_symbols,
    std::string_view description,
    bool postpone_dependencies) noexcept
    : m_name(name)
    , m_has_ord_symbols(has_ord_symbols)
    , m_postpone_dependencies(postpone_dependencies)
    , m_min_operands(min_operands)
    , m_max_operands(max_operands)
    , m_description(std::move(description))
{}

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

static_assert(std::ranges::is_sorted(assembler_instructions, {}, &assembler_instruction::name));

const assembler_instruction* instruction::find_assembler_instructions(std::string_view instr) noexcept
{
    auto it = std::ranges::lower_bound(assembler_instructions, instr, {}, &assembler_instruction::name);
    if (it == std::ranges::end(assembler_instructions) || it->name() != instr)
        return nullptr;
    return std::to_address(it);
}

const assembler_instruction& instruction::get_assembler_instructions(std::string_view instr) noexcept
{
    auto result = find_assembler_instructions(instr);
    assert(result);
    return *result;
}

std::span<const assembler_instruction> instruction::all_assembler_instructions() noexcept
{
    return assembler_instructions;
}

namespace {
struct
{
} constexpr privileged;
struct
{
} constexpr privileged_conditionally;
struct
{
} constexpr has_parameter_list;

template<int arg, int nonzero>
struct branch_argument_t
{};

template<typename T>
struct is_branch_argument_t : std::false_type
{
    using branch_t = branch_argument_t<0, 0>;
};
template<int arg, int nonzero>
struct is_branch_argument_t<branch_argument_t<arg, nonzero>> : std::true_type
{
    using branch_t = branch_argument_t<arg, nonzero>;
};

template<int... arg, int... nonzero>
consteval branch_info_argument select_branch_argument(branch_argument_t<arg, nonzero>...)
{
    constexpr auto res = []() {
        const int args[] = { arg..., 0 };
        const int nonzeros[] = { nonzero..., 0 };
        for (size_t i = 0; i < std::size(args); ++i)
            if (args[i])
                return std::make_pair(args[i], nonzeros[i]);
        return std::make_pair(0, 0);
    }();
    return branch_info_argument { (signed char)res.first, (unsigned char)res.second };
}

constexpr branch_argument_t<-1, 0> branch_argument_unknown;
template<unsigned char nonzero>
requires(nonzero < 4) constexpr branch_argument_t<-1, 1 + nonzero> branch_argument_unknown_nonzero;
template<unsigned char arg>
requires(arg < 4) constexpr branch_argument_t<1 + arg, 0> branch_argument;
template<unsigned char arg, unsigned char nonzero>
requires(arg < 4 && nonzero < 4) constexpr branch_argument_t<1 + arg, 1 + nonzero> branch_argument_nonzero;

struct cc_index
{
    unsigned char value;
};

template<typename... Args>
struct make_machine_instruction_details_args_validator
{
    static constexpr size_t p = (0 + ... + std::is_same_v<Args, std::decay_t<decltype(privileged)>>);
    static constexpr size_t p_c = (0 + ... + std::is_same_v<Args, std::decay_t<decltype(privileged_conditionally)>>);
    static constexpr size_t pl = (0 + ... + std::is_same_v<Args, std::decay_t<decltype(has_parameter_list)>>);
    static constexpr size_t cc = (0 + ... + std::is_same_v<Args, cc_index>);
    static constexpr branch_info_argument ba =
        select_branch_argument(typename is_branch_argument_t<Args>::branch_t()...);
    static constexpr bool value =
        !(p && p_c) && p <= 1 && p_c <= 1 && pl <= 1 && cc <= 1 && (0 + ... + is_branch_argument_t<Args>::value) <= 1;
};

struct
{
    consteval unsigned char operator()(const cc_index& val) const noexcept { return val.value; }
    consteval unsigned char operator()(const auto&) const noexcept { return 0; }
} constexpr cc_visitor;

enum class condition_code_explanation_id : unsigned char
{
#define DEFINE_CC_SET(name, ...) name,
#include "instruction_details.h"
};


#define DEFINE_CC_SET(name, ...)                                                                                       \
    constexpr auto name = cc_index { static_cast<unsigned char>(condition_code_explanation_id::name) };
#include "instruction_details.h"
} // namespace

consteval bool condition_code_explanation::identical(
    const char* t0, const char* t1, const char* t2, const char* t3, size_t n)
{
    return std::equal(t0, t0 + n, t1, t1 + n) && std::equal(t0, t0 + n, t2, t2 + n)
        && std::equal(t0, t0 + n, t3, t3 + n);
}

template<size_t L0, size_t L1, size_t L2, size_t L3, size_t Qual>
consteval condition_code_explanation::condition_code_explanation(const char (&t0)[L0],
    const char (&t1)[L1],
    const char (&t2)[L2],
    const char (&t3)[L3],
    const char (&qualification)[Qual]) noexcept requires(L0 > 0 && L1 > 0 && L2 > 0 && L3 > 0 && Qual > 1 && L0 < 256
                                                            && L1 < 256 && L2 < 256 && L3 < 256 && Qual < 256)
    : text { L0 == 1 ? nullptr : t0,
        L1 == 1 ? nullptr : t1,
        L2 == 1 ? nullptr : t2,
        L3 == 1 ? nullptr : t3,
        qualification }
    , lengths { L0 - 1, L1 - 1, L2 - 1, L3 - 1, Qual - 1 }
    , single_explanation(L0 == L1 && L0 == L2 && L0 == L3 && identical(t0, t1, t2, t3, L0))
{}

template<size_t L0, size_t L1, size_t L2, size_t L3>
consteval condition_code_explanation::condition_code_explanation(
    const char (&t0)[L0], const char (&t1)[L1], const char (&t2)[L2], const char (&t3)[L3]) noexcept
    requires(L0 > 0 && L1 > 0 && L2 > 0 && L3 > 0 && L0 < 256 && L1 < 256 && L2 < 256 && L3 < 256)
    : text { L0 == 1 ? nullptr : t0, L1 == 1 ? nullptr : t1, L2 == 1 ? nullptr : t2, L3 == 1 ? nullptr : t3 }
    , lengths { L0 - 1, L1 - 1, L2 - 1, L3 - 1 }
    , single_explanation(L0 == L1 && L0 == L2 && L0 == L3 && identical(t0, t1, t2, t3, L0))
{}

template<size_t L0>
consteval condition_code_explanation::condition_code_explanation(const char (&t0)[L0]) noexcept
    requires(L0 > 1 && L0 < 256)
    : text { t0, t0, t0, t0 }
    , lengths { L0 - 1, L0 - 1, L0 - 1, L0 - 1 }
    , single_explanation(true)
{}

constinit const condition_code_explanation condition_code_explanations[] = {
#define DEFINE_CC_SET(name, ...) condition_code_explanation(__VA_ARGS__),
#include "instruction_details.h"
};

enum class instruction_fullname
{
#define DEFINE_INSTRUCTION(name, format, page, iset, description, ...) name##_##format,
#include "instruction_details.h"
};

consteval machine_operand_format::machine_operand_format(
    parameter id, parameter first, parameter second, bool optional) noexcept
    : identifier(id)
    , first(first)
    , second(second)
    , optional(optional)
{
    assert(!second.is_empty() || first.is_empty());
}

/*
Rules for displacement operands:
With DB formats
        - must be in format D(B), otherwise throw an error
        - parser returns this in (displacement, 0, base, true) format
With DXB Formats
        - can be either D(X,B) or D(,B) - in this case, the X is replaced with 0
        - parser returns this in (displacement, x, base, false) format
*/
constexpr machine_operand_format db_12_4_U(dis_12u, empty, base_);
constexpr machine_operand_format db_20_4_S(dis_20s, empty, base_);
constexpr machine_operand_format drb_12_4x4_U(dis_12u, idx_reg_r, base_);
constexpr machine_operand_format dxb_12_4x4_U(dis_12u, idx_reg, base_);
constexpr machine_operand_format dxb_20_4x4_S(dis_20s, idx_reg, base_);
constexpr machine_operand_format dxxb_20_4x4_S(dis_idx_20s, idx_reg, base_);
constexpr machine_operand_format dvb_12_5x4_U(dis_12u, vec_reg, base_);
constexpr machine_operand_format reg_4_U(reg, empty, empty);
constexpr machine_operand_format reg_4_U_nz(reg_nz, empty, empty);
constexpr machine_operand_format reg_4_U_2(reg_2, empty, empty);
constexpr machine_operand_format reg_4_U_odd(reg_odd, empty, empty);
constexpr machine_operand_format reg_4_U_even(reg_even, empty, empty);
constexpr machine_operand_format reg_4_U_even_nz(reg_even_nz, empty, empty);
constexpr machine_operand_format mask_4_U(mask, empty, empty);
constexpr machine_operand_format imm_4_U(imm_4u, empty, empty);
constexpr machine_operand_format imm_8_S(imm_8s, empty, empty);
constexpr machine_operand_format imm_8_U(imm_8u, empty, empty);
constexpr machine_operand_format imm_12_S(imm_12s, empty, empty);
constexpr machine_operand_format imm_12_U(imm_12u, empty, empty);
constexpr machine_operand_format imm_16_S(imm_16s, empty, empty);
constexpr machine_operand_format imm_16_U(imm_16u, empty, empty);
constexpr machine_operand_format imm_32_S(imm_32s, empty, empty);
constexpr machine_operand_format imm_32_U(imm_32u, empty, empty);
constexpr machine_operand_format vec_reg_5_U(vec_reg, empty, empty);
constexpr machine_operand_format db_12_8x4L_U(dis_12u, length_8, base_);
constexpr machine_operand_format db_12_4x4L_U(dis_12u, length_4, base_);
constexpr machine_operand_format rel_addr_imm_12_S(reladdr_imm_12s, empty, empty);
constexpr machine_operand_format rel_addr_imm_16_S(reladdr_imm_16s, empty, empty);
constexpr machine_operand_format rel_addr_imm_24_S(reladdr_imm_24s, empty, empty);
constexpr machine_operand_format rel_addr_imm_32_S(reladdr_imm_32s, empty, empty);

// optional variants
constexpr machine_operand_format db_12_4_U_opt(dis_12u, empty, base_, true);
constexpr machine_operand_format db_20_4_S_opt(dis_20s, empty, base_, true);
constexpr machine_operand_format drb_12_4x4_U_opt(dis_12u, idx_reg_r, base_, true);
constexpr machine_operand_format dxb_12_4x4_U_opt(dis_12u, idx_reg, base_, true);
constexpr machine_operand_format dxb_20_4x4_S_opt(dis_20s, idx_reg, base_, true);
constexpr machine_operand_format dvb_12_5x4_U_opt(dis_12u, vec_reg, base_, true);
constexpr machine_operand_format reg_4_U_opt(reg, empty, empty, true);
constexpr machine_operand_format mask_4_U_opt(mask, empty, empty, true);
constexpr machine_operand_format imm_4_U_opt(imm_4u, empty, empty, true);
constexpr machine_operand_format imm_8_S_opt(imm_8s, empty, empty, true);
constexpr machine_operand_format imm_8_U_opt(imm_8u, empty, empty, true);
constexpr machine_operand_format imm_16_U_opt(imm_16u, empty, empty, true);
constexpr machine_operand_format imm_12_S_opt(imm_12s, empty, empty, true);
constexpr machine_operand_format imm_16_S_opt(imm_16s, empty, empty, true);
constexpr machine_operand_format imm_32_S_opt(imm_32s, empty, empty, true);
constexpr machine_operand_format imm_32_U_opt(imm_32u, empty, empty, true);
constexpr machine_operand_format vec_reg_5_U_opt(vec_reg, empty, empty, true);
constexpr machine_operand_format db_12_8x4L_U_opt(dis_12u, length_8, base_, true);
constexpr machine_operand_format db_12_4x4L_U_opt(dis_12u, length_4, base_, true);
constexpr machine_operand_format rel_addr_imm_12_S_opt(reladdr_imm_12s, empty, empty, true);
constexpr machine_operand_format rel_addr_imm_16_S_opt(reladdr_imm_16s, empty, empty, true);
constexpr machine_operand_format rel_addr_imm_24_S_opt(reladdr_imm_24s, empty, empty, true);
constexpr machine_operand_format rel_addr_imm_32_S_opt(reladdr_imm_32s, empty, empty, true);

constinit const machine_operand_format machine_operand_format::empty { {}, {}, {}, false };

namespace {
enum class operand_formats
{
#define DEFINE_INSTRUCTION_FORMAT(name, format, ...) name,
#include "instruction_details.h"
};

constexpr const machine_operand_format _s_operands[] = {
#define DEFINE_INSTRUCTION_FORMAT(name, format, ...) __VA_ARGS__ __VA_OPT__(, )
#include "instruction_details.h"
};

consteval std::span<const machine_operand_format> _ops(unsigned off, unsigned char len) noexcept
{
    return std::span(_s_operands + off, len);
}

consteval unsigned char _count_optional_ops(unsigned off, unsigned char len) noexcept
{
    return (unsigned char)std::ranges::count_if(_ops(off, len), &machine_operand_format::optional);
}

consteval reladdr_transform_mask generate_reladdr_bitmask(
    unsigned offset, unsigned char len, std::span<const mnemonic_transformation> transforms) noexcept
{
    unsigned char result = 0;

    decltype(result) top_bit = 1 << (std::numeric_limits<decltype(result)>::digits - 1);

    auto transforms_b = transforms.begin();
    auto const transforms_e = transforms.end();

    for (size_t processed = 0; const auto& op : _ops(offset, len))
    {
        if (transforms_b != transforms_e && processed == transforms_b->skip)
        {
            assert(op.identifier.type == machine_operand_type::IMM || op.identifier.type == machine_operand_type::MASK
                || op.identifier.type == machine_operand_type::REG
                || op.identifier.type == machine_operand_type::VEC_REG);
            top_bit >>= +!transforms_b++->insert;
            processed = 0;
            continue;
        }

        if (op.identifier.type == machine_operand_type::RELOC_IMM)
            result |= top_bit;

        top_bit >>= 1;
        ++processed;
    }
    return (reladdr_transform_mask)result;
}
} // namespace

constinit const machine_operand_format machine_instruction::s_operands[] = {
#define DEFINE_INSTRUCTION_FORMAT(name, format, ...) __VA_ARGS__ __VA_OPT__(, )
#include "instruction_details.h"
};

constexpr auto operand_map = []() {
    constexpr size_t operand_sizes[] = {
#define DEFINE_INSTRUCTION_FORMAT(name, format, ...)                                                                   \
    std::initializer_list<machine_operand_format> { __VA_ARGS__ }.size(),
#include "instruction_details.h"
    };

    size_t sum = 0;
    std::array<unsigned short, std::size(operand_sizes) + 1> result {};
    for (auto* p = result.data(); auto s : operand_sizes)
    {
        assert(s <= (unsigned char)-1);
        *p++ = (unsigned short)sum;
        sum += s;
    }
    assert(sum <= (unsigned short)-1);
    result.back() = (unsigned short)sum;

    return result;
}();

constinit const char machine_instruction::s_fullnames[] =
#define DEFINE_INSTRUCTION(name, format, page, iset, description, ...) description
#include "instruction_details.h"
    ;

constexpr auto fullname_map = []() {
    constexpr size_t fullname_sizes[] = {
#define DEFINE_INSTRUCTION(name, format, page, iset, description, ...) sizeof(description) - 1,
#include "instruction_details.h"
    };

    size_t sum = 0;
    std::array<unsigned short, std::size(fullname_sizes) + 1> result {};
    for (auto* p = result.data(); auto s : fullname_sizes)
    {
        assert(s <= (unsigned char)-1);
        *p++ = (unsigned short)sum;
        sum += s;
    }
    assert(sum <= (unsigned short)-1);
    result.back() = (unsigned short)sum;

    return result;
}();

template<instruction_fullname fn, typename... Args>
consteval machine_instruction_details make_machine_instruction_details(Args&&... args) noexcept
    requires(make_machine_instruction_details_args_validator<std::decay_t<Args>...>::value)
{
    constexpr auto name = fullname_map[(int)fn];
    constexpr auto len = fullname_map[(int)fn + 1] - name;
    using A = make_machine_instruction_details_args_validator<std::decay_t<Args>...>;
    return machine_instruction_details {
        name, len, static_cast<unsigned char>((0 + ... + cc_visitor(args))), A::p > 0, A::p_c > 0, A::pl > 0, A::ba
    };
}

template<mach_format F, operand_formats op_format>
class instruction_format_definition_factory
{
public:
    static constexpr instruction_format_definition def() noexcept
    {
        constexpr auto offset = operand_map[(int)op_format];
        constexpr auto len = operand_map[(int)op_format + 1] - offset;

        return { offset, len, F };
    }
};

consteval char machine_instruction::get_length_by_format(mach_format instruction_format) noexcept
{
    auto interval = static_cast<int>(instruction_format);
    if (interval >= static_cast<int>(mach_format::length_48))
        return static_cast<char>(size_identifier::LENGTH_48);
    if (interval >= static_cast<int>(mach_format::length_32))
        return static_cast<char>(size_identifier::LENGTH_32);
    if (interval >= static_cast<int>(mach_format::length_16))
        return static_cast<char>(size_identifier::LENGTH_16);
    return static_cast<char>(size_identifier::LENGTH_0);
}

consteval machine_instruction::machine_instruction(std::string_view name,
    mach_format format,
    unsigned short operand_offset,
    unsigned char operand_len,
    unsigned short page_no,
    instruction_set_affiliation instr_set_affiliation,
    machine_instruction_details d) noexcept
    : m_name(name)
    , m_size_identifier(get_length_by_format(format))
    , m_page_no(page_no)
    , m_instr_set_affiliation(instr_set_affiliation)
    , m_format(format)
    , m_reladdr_mask(generate_reladdr_bitmask(operand_offset, operand_len, {}))
    , m_optional_op_count(_count_optional_ops(operand_offset, operand_len))
    , m_operand_len(operand_len)
    , m_operands_offset(operand_offset)
    , m_fullname_offset(d.fullname_offset)
    , m_fullname_length(d.fullname_length)
    , m_cc_explanation(d.cc_explanation)
    , m_privileged(d.privileged)
    , m_privileged_conditionally(d.privileged_conditionally)
    , m_has_parameter_list(d.has_parameter_list)
    , m_branch_argument(d.branch_argument)
{
    assert(operand_len <= max_operand_count);
}

consteval machine_instruction::machine_instruction(std::string_view name,
    instruction_format_definition ifd,
    unsigned short page_no,
    instruction_set_affiliation instr_set_affiliation,
    machine_instruction_details d) noexcept
    : machine_instruction(name, ifd.format, ifd.op_format_offset, ifd.op_format_len, page_no, instr_set_affiliation, d)
{}

#define DEFINE_INSTRUCTION_FORMAT(name, format, ...)                                                                   \
    constexpr auto name = instruction_format_definition_factory<format, operand_formats::name>::def();
#include "instruction_details.h"

#define DEFINE_INSTRUCTION(name, format, page, iset, description, ...)                                                 \
    { #name, format, page, iset, make_machine_instruction_details<instruction_fullname::name##_##format>(__VA_ARGS__) },
constexpr machine_instruction machine_instructions[] = {
#include "instruction_details.h"
};

static_assert(std::ranges::is_sorted(machine_instructions, {}, &machine_instruction::name));

namespace {
consteval bool check_machine_instruction_overlap(std::span<const machine_instruction> instrs)
{
    for (size_t i = 0; i < instrs.size() - 1; ++i)
    {
        const auto initial_name = instrs[i].name();
        if (initial_name != instrs[i + 1].name())
            continue;
        bool archs[1 << arch_bitfield_width] = {};
        bool esa = false;
        bool xa = false;
        bool s370 = false;
        bool dos = false;
        bool uni = false;
        for (; i < instrs.size() && instrs[i].name() == initial_name; ++i)
        {
            const auto af = instrs[i].instr_set_affiliation();
            if (af.esa && std::exchange(esa, true))
                return false;
            if (af.xa && std::exchange(xa, true))
                return false;
            if (af._370 && std::exchange(s370, true))
                return false;
            if (af.dos && std::exchange(dos, true))
                return false;
            if (af.uni && std::exchange(uni, true))
                return false;

            for (auto j = (int)af.z_arch; j < (int)af.z_arch_removed; ++j)
                if (std::exchange(archs[j], true))
                    return false;
        }
        --i;
    }

    return true;
}
} // namespace

static_assert(check_machine_instruction_overlap(machine_instructions), "Overlap detected in machine instruction list");

const machine_instruction* instruction::find_machine_instructions(std::string_view name) noexcept
{
    auto it = std::ranges::lower_bound(machine_instructions, name, {}, &machine_instruction::name);
    if (it == std::ranges::end(machine_instructions) || it->name() != name)
        return nullptr;
    return std::to_address(it);
}

const machine_instruction& instruction::get_machine_instructions(std::string_view name) noexcept
{
    auto mi = find_machine_instructions(name);
    assert(mi);
    return *mi;
}

std::span<const machine_instruction> instruction::all_machine_instructions() noexcept { return machine_instructions; }


consteval mnemonic_transformation::mnemonic_transformation(unsigned short v) noexcept
    : value(v)
{}

consteval mnemonic_transformation::mnemonic_transformation(unsigned char skip, unsigned short v, bool insert) noexcept
    : skip(skip)
    , insert(insert)
    , value(v)
{
    assert(skip < machine_instruction::max_operand_count);
}

consteval mnemonic_transformation::mnemonic_transformation(
    unsigned char skip, mnemonic_transformation_kind t, unsigned char src) noexcept
    : skip(skip)
    , source(src)
    , type(t)
{
    assert(t == mnemonic_transformation_kind::copy);
    assert(skip < machine_instruction::max_operand_count);
    assert(src < machine_instruction::max_operand_count);
}

consteval mnemonic_transformation::mnemonic_transformation(
    unsigned char skip, unsigned short v, mnemonic_transformation_kind t, unsigned char src, bool insert) noexcept
    : skip(skip)
    , source(src)
    , type(t)
    , insert(insert)
    , value(v)
{
    assert(t != mnemonic_transformation_kind::copy && t != mnemonic_transformation_kind::value);
    assert(skip < machine_instruction::max_operand_count);
    assert(src < machine_instruction::max_operand_count);
}

consteval mnemonic_code::mnemonic_code(std::string_view name,
    const machine_instruction* instr,
    instruction_set_affiliation instr_set_affiliation,
    std::initializer_list<const mnemonic_transformation> transform) noexcept
    : m_instruction(instr)
    , m_transform {}
    , m_transform_count((unsigned char)transform.size())
    , m_reladdr_mask(generate_reladdr_bitmask(instr->m_operands_offset, instr->m_operand_len, transform))
    , m_instr_set_affiliation(instr_set_affiliation)
    , m_name(name)
{
    assert(transform.size() <= m_transform.size());
    std::ranges::copy(transform, m_transform.begin());
    const auto insert_count = std::ranges::count_if(transform, [](auto t) { return t.insert; });
    [[maybe_unused]] const auto total = std::accumulate(
        transform.begin(), transform.end(), (size_t)0, [](size_t res, auto t) { return res + t.skip + t.insert; });
    assert(total <= instr->m_operand_len);

    m_op_max = instr->m_operand_len - insert_count;
    m_op_min = instr->m_operand_len - instr->m_optional_op_count - insert_count;
    assert(m_op_max <= instr->m_operand_len);
    assert(m_op_min <= m_op_max);

    for ([[maybe_unused]] const auto& r : transform)
        assert(!r.has_source() || r.source < m_op_max);
}

namespace {
consteval const machine_instruction* find_mi(std::string_view name) noexcept
{
    auto it = std::ranges::lower_bound(machine_instructions, name, {}, &machine_instruction::name);
    assert(it != std::ranges::end(machine_instructions) && it->name() == name);
    return std::to_address(it);
}

template<std::array<char, 8> name>
struct mi_locator
{
    static constexpr const machine_instruction* value = find_mi(std::string_view(name.data()));
    static_assert(value != nullptr, "Unable to find the parent instruciton");
};
} // namespace

constexpr mnemonic_code mnemonic_codes[] = {
#define DEFINE_MNEMONIC(name, parent, version, ...) { #name, mi_locator<{ #parent }>::value, version, __VA_ARGS__ },
#include "instruction_details.h"
};

static_assert(std::ranges::is_sorted(mnemonic_codes, {}, &mnemonic_code::name));

namespace {
consteval bool instr_and_mnemo_is_distinct()
{
    auto i = std::begin(machine_instructions);
    const auto ie = std::end(machine_instructions);
    auto m = std::begin(mnemonic_codes);
    const auto me = std::end(mnemonic_codes);

    while (i != ie && m != me)
    {
        if (const auto c = i->name() <=> m->name(); c < 0)
            ++i;
        else if (c > 0)
            ++m;
        else
            return false;
    }
    return true;
}
static_assert(instr_and_mnemo_is_distinct(), "Collision between instructions and mnemonics");
} // namespace

const mnemonic_code* instruction::find_mnemonic_codes(std::string_view name) noexcept
{
    auto it = std::ranges::lower_bound(mnemonic_codes, name, {}, &mnemonic_code::name);
    if (it == std::ranges::end(mnemonic_codes) || it->name() != name)
        return nullptr;
    return std::to_address(it);
}

const mnemonic_code& instruction::get_mnemonic_codes(std::string_view name) noexcept
{
    auto result = find_mnemonic_codes(name);
    assert(result);
    return *result;
}

std::span<const mnemonic_code> instruction::all_mnemonic_codes() noexcept { return mnemonic_codes; }

namespace {
consteval instruction_set_size compute_instruction_set_size(instruction_set_version v)
{
    instruction_set_size result = {
        0,
        0,
        std::size(ca_instructions),
        std::size(assembler_instructions),
    };
    for (const auto& i : machine_instructions)
        if (instruction_available(i.instr_set_affiliation(), v))
            ++result.machine;
    for (const auto& i : mnemonic_codes)
        if (instruction_available(i.instr_set_affiliation(), v))
            ++result.mnemonic;

    return result;
}

template<instruction_set_version v>
struct instruction_set_size_generator
{
    static constexpr auto value = compute_instruction_set_size(v);
};

constexpr instruction_set_size instruction_set_sizes[] = {
    {},
    instruction_set_size_generator<instruction_set_version::ZOP>::value,
    instruction_set_size_generator<instruction_set_version::YOP>::value,
    instruction_set_size_generator<instruction_set_version::Z9>::value,
    instruction_set_size_generator<instruction_set_version::Z10>::value,
    instruction_set_size_generator<instruction_set_version::Z11>::value,
    instruction_set_size_generator<instruction_set_version::Z12>::value,
    instruction_set_size_generator<instruction_set_version::Z13>::value,
    instruction_set_size_generator<instruction_set_version::Z14>::value,
    instruction_set_size_generator<instruction_set_version::Z15>::value,
    instruction_set_size_generator<instruction_set_version::Z16>::value,
    instruction_set_size_generator<instruction_set_version::Z17>::value,
    instruction_set_size_generator<instruction_set_version::ESA>::value,
    instruction_set_size_generator<instruction_set_version::XA>::value,
    instruction_set_size_generator<instruction_set_version::_370>::value,
    instruction_set_size_generator<instruction_set_version::DOS>::value,
    instruction_set_size_generator<instruction_set_version::UNI>::value,
};
} // namespace

const instruction_set_size& get_instruction_sizes(instruction_set_version v) noexcept
{
    const auto idx = static_cast<unsigned char>(v);
    assert(0 < idx && idx < std::size(instruction_set_sizes));
    return instruction_set_sizes[idx];
}

const instruction_set_size& get_instruction_sizes() noexcept
{
    static constexpr instruction_set_size result = {
        std::size(mnemonic_codes),
        std::size(machine_instructions),
        std::size(ca_instructions),
        std::size(assembler_instructions),
    };
    return result;
}
} // namespace hlasm_plugin::parser_library::context
