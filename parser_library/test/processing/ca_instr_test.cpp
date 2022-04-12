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

#include "gtest/gtest.h"

#include "../common_testing.h"

using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::parser_library::context;
using namespace hlasm_plugin::parser_library::semantics;

// tests for
// variable substitution for model statements
// concatenation of multiple substitutions
// CA instructions

TEST(var_subs, gbl_instr_only)
{
    std::string input("   GBLA VAR");
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "VAR"), 0);
}

TEST(var_subs, lcl_instr_only)
{
    std::string input("   LCLA VAR");
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "VAR"), 0);
}

TEST(var_subs, gbl_instr_more)
{
    std::string input("   GBLA VAR,VAR2,VAR3");
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "VAR"), 0);
    EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "VAR2"), 0);
    EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "VAR3"), 0);
}

TEST(var_subs, lcl_instr_more)
{
    std::string input("   LCLA VAR,VAR2,VAR3");
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "VAR"), 0);
    EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "VAR2"), 0);
    EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "VAR3"), 0);
}

TEST(var_subs, big_arrays)
{
    std::string input = R"(
    LCLC &LARR(100000000)
    GBLC &GARR(100000000)
)";
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_vector<C_t>(a.hlasm_ctx(), "LARR")->size(), 0);
    EXPECT_EQ(get_var_vector<C_t>(a.hlasm_ctx(), "GARR")->size(), 0);
}

TEST(var_subs, set_to_var)
{
    std::string input("&VAR SETA 3");
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "VAR"), 3);
}

TEST(var_subs, set_to_var_idx)
{
    std::string input("&VAR(2) SETA 3");
    analyzer a(input);
    a.analyze();

    std::unordered_map<size_t, A_t> expected = { { 1, 3 } };
    EXPECT_THAT(*get_var_vector_map<A_t>(a.hlasm_ctx(), "VAR"), ::testing::ContainerEq(expected));
}

TEST(var_subs, set_to_var_idx_many)
{
    std::string input("&VAR(2) SETA 3,4,5");
    analyzer a(input);
    a.analyze();

    std::unordered_map<size_t, A_t> expected = { { 1, 3 }, { 2, 4 }, { 3, 5 } };
    EXPECT_THAT(*get_var_vector_map<A_t>(a.hlasm_ctx(), "VAR"), ::testing::ContainerEq(expected));
}

TEST(var_subs, var_sym_reset)
{
    std::string input = R"(
&VAR SETC 'avc'   
&VAR SETC 'XXX'
)";
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "VAR"), "XXX");
}

TEST(var_subs, created_set_sym)
{
    std::string input = R"(
&VAR SETC 'avc'   
&VAR2 SETB 0  
&(ab&VAR.cd&VAR2) SETA 11
)";
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "abavccd0"), 11);
}

TEST(var_subs, instruction_substitution_space_at_end)
{
    std::string input = R"(
&VAR SETC 'LR '   
     &VAR 1,1
)";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_EQ(a.diags().size(), (size_t)0);
}

TEST(var_subs, instruction_substitution_space_in_middle)
{
    std::string input = R"(
&VAR SETC 'LR 1,1'   
     &VAR 
)";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "E075" }));
}

TEST(var_concatenation, concatenated_string_dot_last)
{
    std::string input = R"(
&VAR SETC 'avc'   
&VAR2 SETC '&VAR.'
)";
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "VAR2"), "avc");
}

TEST(var_concatenation, concatenated_string_dot)
{
    std::string input = R"(
&VAR SETC 'avc'   
&VAR2 SETC '&VAR.-get'
)";
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "VAR2"), "avc-get");
}

TEST(var_concatenation, concatenated_string_double_dot)
{
    std::string input = R"(
&VAR SETC 'avc'   
&VAR2 SETC '&VAR..'
)";
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "VAR2"), "avc.");
}

TEST(AGO, extended)
{
    std::string input(R"(
 AGO (2).a,.b,.c
.A ANOP   
&VAR1 SETB 0
.B ANOP
&VAR2 SETB 0
.C ANOP
&VAR3 SETB 0
)");
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<B_t>(a.hlasm_ctx(), "VAR1"), std::nullopt);
    EXPECT_EQ(get_var_value<B_t>(a.hlasm_ctx(), "VAR2"), false);
    EXPECT_EQ(get_var_value<B_t>(a.hlasm_ctx(), "VAR3"), false);
}

TEST(AGO, extended_fail)
{
    std::string input(R"(
 AGO (8).a,.b,.c
.A ANOP   
&VAR1 SETB 0
.B ANOP
&VAR2 SETB 0
.C ANOP
&VAR3 SETB 0
)");
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<B_t>(a.hlasm_ctx(), "VAR1"), false);
    EXPECT_EQ(get_var_value<B_t>(a.hlasm_ctx(), "VAR2"), false);
    EXPECT_EQ(get_var_value<B_t>(a.hlasm_ctx(), "VAR3"), false);
}

TEST(AIF, extended)
{
    std::string input(R"(
 AIF (0).a,(1).b,(1).c
.A ANOP   
&VAR1 SETB 0
.B ANOP
&VAR2 SETB 0
.C ANOP
&VAR3 SETB 0
)");
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<B_t>(a.hlasm_ctx(), "VAR1"), std::nullopt);
    EXPECT_EQ(get_var_value<B_t>(a.hlasm_ctx(), "VAR2"), false);
    EXPECT_EQ(get_var_value<B_t>(a.hlasm_ctx(), "VAR3"), false);
}

TEST(AIF, extended_fail)
{
    std::string input(R"(
 AIF (0).a,(0).b,(0).c
.A ANOP   
&VAR1 SETB 0
.B ANOP
&VAR2 SETB 0
.C ANOP
&VAR3 SETB 0
)");
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<B_t>(a.hlasm_ctx(), "VAR1"), false);
    EXPECT_EQ(get_var_value<B_t>(a.hlasm_ctx(), "VAR2"), false);
    EXPECT_EQ(get_var_value<B_t>(a.hlasm_ctx(), "VAR3"), false);
}

TEST(ACTR, exceeded)
{
    std::string input(R"(
.A ANOP
 LR 1,1
 AGO .A
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "E056" }));
}

TEST(ACTR, infinite_ACTR)
{
    std::string input(R"(
.A ANOP
 ACTR 1024
 LR 1,1
 AGO .A
)");
    analyzer a(input, analyzer_options(asm_option { .statement_count_limit = 10000 }));
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "W063", "E077" }));
}

TEST(ACTR, negative)
{
    std::string input(R"(
&A SETA -2147483648
   ACTR &A
   AGO .A
.A ANOP
&B SETA 1
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "E056" }));
    EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "B"), std::nullopt);
}

TEST(MHELP, SYSNDX_limit)
{
    std::string input = R"(
         GBLC &LASTNDX
         MACRO
         MAC
         GBLC &LASTNDX
&LASTNDX SETC '&SYSNDX'
         MEND

         MHELP 256
&I       SETA  0
.NEXT    AIF   (&I GT 256).DONE
&I       SETA  &I+1
         MAC
         AGO   .NEXT
.DONE    ANOP  ,
 )";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "E072" }));
    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "LASTNDX"), "0256");
}

TEST(MHELP, invalid_operands)
{
    std::string input = R"(
 MHELP
 MHELP 1,1
 MHELP ,
 MHELP ABC
 MHELP (1).ABC
ABC EQU 1
)";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "E021", "E020", "E020", "CE012", "E010" }));
}

TEST(MHELP, valid_operands)
{
    std::string input = R"(
ABC EQU 1
&VAR SETA 1
 MHELP 1
 MHELP X'1'
 MHELP B'1'
 MHELP ABC
 MHELP ABC+ABC
 MHELP ABC*5
 MHELP &VAR+1
 MHELP &VAR*&VAR
)";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(a.diags().empty());
}

TEST(SET, conversions_valid)
{
    std::string input(R"(
&A SETA 1
&B SETB 0
&C SETC '2'

&A SETA &B
&A SETA &C

&C SETC '&A'
&C SETC '&B'
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_EQ(a.diags().size(), (size_t)0);
}

TEST(SET, conversions_invalid)
{
    std::string input(R"(
&A SETA 1
&B SETB 0
&C SETC '2'

&A SETA '1'
&B SETB ('1')

&C SETC &A
&C SETC &B
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(
        matches_message_codes(a.diags(), { "CE004", "CE004", "CE004", "CE004", "CE004", "CE004", "CE017", "CE017" }));
}

TEST(CA_instructions, undefined_relocatable)
{
    std::string input(R"(
A EQU B
L1 LR 1,1
L2 LR 1,1

&V1 SETA L2-L1
&V2 SETA A

B EQU 1
)");
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "CE012", "CE012", "CE012" }));
}

TEST(var_subs, defined_by_self_ref)
{
    std::string input("&VAR(N'&VAR+1) SETA N'&VAR+1");
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(a.diags().empty());

    EXPECT_EQ(get_var_vector<A_t>(a.hlasm_ctx(), "VAR"), std::vector<A_t> { 2 });
}
