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

// tests parsing of hlasm strings

TEST(parser, mach_string_double_ampersand)
{
    std::string input("A EQU C'&&'");
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_symbol_abs(a.hlasm_ctx(), "A"), 80);
}

TEST(parser, ca_string_double_ampersand)
{
    std::string input("&A SETC '&&'");
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(get_var_value<context::C_t>(a.hlasm_ctx(), "A"), "&&");
}

namespace {

struct test_param
{
    std::string name;
    std::string parameter;
    std::string expected;
};

struct stringer
{
    std::string operator()(::testing::TestParamInfo<test_param> p) { return p.param.name; }
};

class parser_string_fixture : public ::testing::TestWithParam<test_param>
{};

INSTANTIATE_TEST_SUITE_P(parser,
    parser_string_fixture,
    ::testing::Values(test_param { "A_no_attr", "A'SYM 93'", "A'SYM 93'" },
        test_param { "D_attr", "D'SYM 93'", "D'SYM 93'" },
        test_param { "I_attr", "I'SYM 93'", "I'SYM" },
        test_param { "K_attr", "K'SYM 93'", "K'SYM 93'" },
        test_param { "L_attr", "L'SYM 93'", "L'SYM" },
        test_param { "N_attr", "N'SYM 93'", "N'SYM 93'" },
        test_param { "O_attr", "O'SYM 93'", "O'SYM" },
        test_param { "S_attr", "S'SYM 93'", "S'SYM" },
        test_param { "T_attr", "T'SYM 93'", "T'SYM" },
        test_param { "attr_and_string", "S'SYM' STH'", "S'SYM' STH'" },
        test_param { "literal_FD", "=FD'SYM STH'", "=FD'SYM STH'" },
        test_param { "literal_FS", "=FS'SYM STH'", "=FS'SYM STH'" },
        test_param { "number_before_attr_L", "=4L'SYM 93'", "=4L'SYM 93'" },
        test_param { "quote_before_attr_L", "\"L'SYM 93'", "\"L'SYM" },
        test_param { "quote_before_attr_D", "\"D'SYM 93'", "\"D'SYM 93'" }),
    stringer());
} // namespace

TEST_P(parser_string_fixture, basic)
{
    std::string input = R"(
 GBLC &PAR
 MACRO
 MAC &VAR
 GBLC &PAR
&PAR SETC '&VAR'
 MEND
 
 MAC )" + GetParam().parameter;
    analyzer a(input);
    a.analyze();

    a.collect_diags();
    EXPECT_TRUE(a.diags().empty());

    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "PAR"), GetParam().expected);
}

TEST(parser, incomplete_string)
{
    std::string input = R"(
 GBLC &PAR
 MACRO
 MAC &VAR
 GBLC &PAR
&PAR SETC '&VAR'
 MEND
 
 MAC 'A 93)";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "S0005" }));

    auto par_value = get_var_value<C_t>(a.hlasm_ctx(), "PAR");
    ASSERT_TRUE(par_value.has_value());
}

TEST(parser, preserve_structured_parameter)
{
    std::string input = R"(
     GBLC  &PAR
     MACRO
     MAC2
     GBLC  &PAR
&PAR SETC  '&SYSLIST(1,1)'
     MEND

     MACRO
     MAC   &P1
     MAC2  &P1
     MEND


     MAC   (A,O'-9')
)";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(a.diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "PAR"), "A");
}

TEST(parser, preserve_structured_parameter_2)
{
    std::string input = R"(
     GBLC  &PAR
     MACRO
     MAC2
     GBLC  &PAR
&PAR SETC  '&SYSLIST(1,1)'
     MEND

     MACRO
     MAC   &P1
     MAC2  &P1.
     MEND


     MAC   (A,O'-9')
)";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(a.diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "PAR"), "A");
}

namespace {
struct test_param_parser_quotes
{
    std::string name;
    bool is_consuming;
};

struct stringer_test_param_parser_quotes
{
    std::string operator()(::testing::TestParamInfo<test_param_parser_quotes> p) { return p.param.name; }
};

class parser_quotes_fixture : public ::testing::TestWithParam<test_param_parser_quotes>
{};

INSTANTIATE_TEST_SUITE_P(parser,
    parser_quotes_fixture,
    ::testing::Values(test_param_parser_quotes { "A", false },
        test_param_parser_quotes { "B", false },
        test_param_parser_quotes { "C", false },
        test_param_parser_quotes { "D", false },
        test_param_parser_quotes { "E", false },
        test_param_parser_quotes { "F", false },
        test_param_parser_quotes { "G", false },
        test_param_parser_quotes { "H", false },
        test_param_parser_quotes { "I", true },
        test_param_parser_quotes { "J", false },
        test_param_parser_quotes { "K", false },
        test_param_parser_quotes { "L", true },
        test_param_parser_quotes { "M", false },
        test_param_parser_quotes { "N", false },
        test_param_parser_quotes { "O", true },
        test_param_parser_quotes { "P", false },
        test_param_parser_quotes { "Q", false },
        test_param_parser_quotes { "R", false },
        test_param_parser_quotes { "S", true },
        test_param_parser_quotes { "T", true },
        test_param_parser_quotes { "U", false },
        test_param_parser_quotes { "V", false },
        test_param_parser_quotes { "W", false },
        test_param_parser_quotes { "X", false },
        test_param_parser_quotes { "Y", false },
        test_param_parser_quotes { "Z", false }),
    stringer_test_param_parser_quotes());
} // namespace

TEST_P(parser_quotes_fixture, no_brackets)
{
    std::string input = R"(
         MACRO
         MAC &VAR
         GBLC &STR
&STR     SETC '&VAR'
         MEND
 
         GBLC &STR
&INSTR   SETC   'J'
         MAC )"
        + GetParam().name + "'&INSTR";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a.diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "STR"), GetParam().name + "'J");
    }
    else
        EXPECT_TRUE(matches_message_codes(a.diags(), { "S0005" }));
}

TEST_P(parser_quotes_fixture, no_brackets_remark)
{
    std::string input = R"(
         MACRO
         MAC &VAR
         GBLC &STR
&STR     SETC '&VAR'
         MEND
 
         GBLC &STR
&INSTR   SETC   'J'
         MAC )"
        + GetParam().name + "'&INSTR          REMARK";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a.diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "STR"), GetParam().name + "'J");
    }
    else
        EXPECT_TRUE(matches_message_codes(a.diags(), { "S0005" }));
}


TEST_P(parser_quotes_fixture, no_brackets_remark_2)
{
    std::string input = R"(
         MACRO
         MAC &VAR
         GBLC &STR
&STR     SETC '&VAR'
         MEND
 
         GBLC &STR
&INSTR   SETC   'J'
         MAC )"
        + GetParam().name + "'&INSTR          REMARK'";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(a.diags().empty());
    if (GetParam().is_consuming)
        EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "STR"), GetParam().name + "'J");
    else
        EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "STR"), GetParam().name + "'J          REMARK'");
}

TEST_P(parser_quotes_fixture, brackets)
{
    std::string input = R"(
         MACRO
         MAC &VAR
         GBLC &STR
&STR     SETC '&VAR(1)'
         MEND
 
         GBLC &STR
&INSTR   SETC   'J'
         MAC ()"
        + GetParam().name + "'&INSTR)";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a.diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "STR"), GetParam().name + "'J");
    }
    else
        EXPECT_TRUE(matches_message_codes(a.diags(), { "S0005" }));
}

TEST_P(parser_quotes_fixture, brackets_2_params)
{
    std::string input = R"(
         MACRO
         MAC &VAR
         GBLC &STR1,&STR2
&STR1    SETC '&VAR(1)'
&STR2    SETC '&VAR(2)'
         MEND
 
         GBLC &STR1,&STR2
         MAC (A,)"
        + GetParam().name + "'-9')";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(a.diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "STR1"), "A");
    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "STR2"), GetParam().name + "'-9'");
}

TEST_P(parser_quotes_fixture, no_ending_apostrophe)
{
    std::string input = R"(
         MACRO
         MAC &VAR
         GBLC &STR
&STR     SETC '&VAR'
         MEND
 
         GBLC &STR
         MAC ")"
        + GetParam().name + "'SYM";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a.diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "STR"), "\"" + GetParam().name + "'SYM");
    }
    else
        EXPECT_TRUE(matches_message_codes(a.diags(), { "S0005" }));
}

TEST_P(parser_quotes_fixture, no_ending_apostrophe_2)
{
    std::string input = R"(
         MACRO
         MAC &VAR
         GBLC &STR
&STR     SETC '&VAR'
         MEND
 
         GBLC &STR
         MAC ")"
        + GetParam().name + "'SYM' STH";
    analyzer a(input);
    a.analyze();
    a.collect_diags();

    if (GetParam().is_consuming)
        EXPECT_TRUE(matches_message_codes(a.diags(), { "S0005" }));
    else
    {
        EXPECT_TRUE(a.diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "STR"), "\"" + GetParam().name + "'SYM'");
    }
}