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
#include "../mock_parse_lib_provider.h"

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

namespace {
struct test_param_parser_attribute
{
    std::string name;
    bool is_consuming;
};

struct stringer_test_param_parser_attribute
{
    std::string operator()(::testing::TestParamInfo<test_param_parser_attribute> p) { return p.param.name; }
};

class parser_attribute_fixture : public ::testing::TestWithParam<test_param_parser_attribute>
{};

INSTANTIATE_TEST_SUITE_P(parser,
    parser_attribute_fixture,
    ::testing::Values(test_param_parser_attribute { "A", false }, // Intentionally not an attribute
        test_param_parser_attribute { "D", false },
        test_param_parser_attribute { "I", true },
        test_param_parser_attribute { "K", false },
        test_param_parser_attribute { "L", true },
        test_param_parser_attribute { "N", false },
        test_param_parser_attribute { "O", true },
        test_param_parser_attribute { "S", true },
        test_param_parser_attribute { "T", true }),
    stringer_test_param_parser_attribute());

mock_parse_lib_provider lib_provider { { "MAC", R"(*
         MACRO
         MAC &VAR
         GBLC &STR
&STR     SETC '&VAR'
         MEND
)" },
    { "MAC_LIST_1_ELEM", R"(*
         MACRO
         MAC_LIST_1_ELEM &VAR
         GBLC &STR
&STR     SETC '&VAR(1)'
         MEND
)" },
    { "MAC_LIST_2_ELEM", R"(*
         MACRO
         MAC_LIST_2_ELEM &VAR
         GBLC &STR1,&STR2
&STR1     SETC '&VAR(1)'
&STR2     SETC '&VAR(2)'
         MEND
)" } };

std::unique_ptr<analyzer> analyze(const std::string& s)
{
    auto a = std::make_unique<analyzer>(s, analyzer_options { &lib_provider });

    a->analyze();
    a->collect_diags();

    return a;
}

} // namespace

TEST_P(parser_attribute_fixture, missing_apostrophe)
{
    std::string input = R"(
         GBLC &STR
         MAC )"
        + GetParam().name + "'";

    auto a = analyze(input);

    EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, instr_0_end_apostrophes)
{
    std::string input = R"(
         GBLC &STR
&INSTR   SETC   'J'
         MAC )"
        + GetParam().name + "'&INSTR";

    auto a = analyze(input);

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J");
    }
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, instr_0_end_apostrophe_remark)
{
    std::string input = R"( 
         GBLC &STR
&INSTR   SETC   'J'
         MAC )"
        + GetParam().name + "'&INSTR          REMARK";

    auto a = analyze(input);

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J");
    }
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, text_0_end_apostrophes)
{
    std::string input = R"(
         GBLC &STR
         MAC )"
        + GetParam().name + "'J";

    auto a = analyze(input);

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J");
    }
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, text_0_end_apostrophe_remark)
{
    std::string input = R"(
         GBLC &STR
         MAC )"
        + GetParam().name + "'J          REMARK";

    auto a = analyze(input);

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J");
    }
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, number_variants_0_end_apostrophes)
{
    std::string input = R"(
         GBLC &STR
         MAC )"
        + GetParam().name + R"('9
         MAC )"
        + GetParam().name + R"('9           REMARK
         MAC )"
        + GetParam().name + R"('-9
         MAC )"
        + GetParam().name + R"('-9          REMARK
        
)";

    auto a = analyze(input);

    EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005", "S0005", "S0005", "S0005" }));
}

TEST_P(parser_attribute_fixture, instr_1_end_apostrophe)
{
    std::string input = R"(
         GBLC &STR
&INSTR   SETC   'J'
         MAC )"
        + GetParam().name + "'&INSTR'";

    auto a = analyze(input);

    EXPECT_TRUE(a->diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J'");
}

TEST_P(parser_attribute_fixture, instr_1_end_apostrophe_remark)
{
    std::string input = R"(
         GBLC &STR
&INSTR   SETC   'J'
         MAC )"
        + GetParam().name + "'&INSTR          REMARK'";

    auto a = analyze(input);

    EXPECT_TRUE(a->diags().empty());
    if (GetParam().is_consuming)
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J");
    else
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J          REMARK'");
}

TEST_P(parser_attribute_fixture, text_1_end_apostrophe)
{
    std::string input = R"(
         GBLC &STR
         MAC )"
        + GetParam().name + "'J'";

    auto a = analyze(input);

    if (GetParam().is_consuming)
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
    else
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J'");
    }
}

TEST_P(parser_attribute_fixture, text_1_end_apostrophe_text_remark)
{
    std::string input = R"(
         GBLC &STR
         MAC )"
        + GetParam().name + "'J          REMARK'";

    auto a = analyze(input);

    EXPECT_TRUE(a->diags().empty());
    if (GetParam().is_consuming)
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J");
    else
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J          REMARK'");
}

TEST_P(parser_attribute_fixture, number_1_end_apostrophe)
{
    std::string input = R"(
         GBLC &STR
         MAC )"
        + GetParam().name + "'9'";

    auto a = analyze(input);

    EXPECT_TRUE(a->diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'9'");
}

TEST_P(parser_attribute_fixture, number_1_end_apostrophe_remark)
{
    std::string input = R"(
         GBLC &STR
         MAC )"
        + GetParam().name + "'9           REMARK'";

    auto a = analyze(input);

    EXPECT_TRUE(a->diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'9           REMARK'");
}

TEST_P(parser_attribute_fixture, negative_number_1_end_apostrophe)
{
    std::string input = R"(
         GBLC &STR
         MAC )"
        + GetParam().name + "'-9'";

    auto a = analyze(input);

    EXPECT_TRUE(a->diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'-9'");
}

TEST_P(parser_attribute_fixture, negative_number_1_end_apostrophe_remark)
{
    std::string input = R"(
         GBLC &STR
         MAC )"
        + GetParam().name + "'-9          REMARK'";

    auto a = analyze(input);

    EXPECT_TRUE(a->diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'-9          REMARK'");
}

TEST_P(parser_attribute_fixture, instr_2_end_apostrophes)
{
    std::string input = R"( 
         GBLC &STR
&INSTR   SETC   'J'
         MAC )"
        + GetParam().name + "'&INSTR''";

    auto a = analyze(input);

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J''");
    }
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, instr_2_end_apostrophes_remark)
{
    std::string input = R"(
         GBLC &STR
&INSTR   SETC   'J'
         MAC )"
        + GetParam().name + "'&INSTR          REMARK''";

    auto a = analyze(input);

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J");
    }
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, text_2_end_apostrophes)
{
    std::string input = R"(
         GBLC &STR
         MAC )"
        + GetParam().name + "'J''";

    auto a = analyze(input);

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J''");
    }
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, text_2_end_apostrophes_remark)
{
    std::string input = R"(
         GBLC &STR
         MAC )"
        + GetParam().name + "'J          REMARK''";

    auto a = analyze(input);

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J");
    }
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, number_variants_2_end_apostrophes)
{
    std::string input = R"(
         GBLC &STR
         MAC )"
        + GetParam().name + R"('9''
         MAC )"
        + GetParam().name + R"('9           REMARK''
         MAC )"
        + GetParam().name + R"('-9''
         MAC )"
        + GetParam().name + R"('-9          REMARK''
)";

    auto a = analyze(input);

    EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005", "S0005", "S0005", "S0005" }));
}

TEST_P(parser_attribute_fixture, list_1_elem_var_instr)
{
    std::string input = R"(
&VAR     SETC 'J'
         GBLC &STR
         MAC_LIST_1_ELEM ()"
        + GetParam().name + "'&VAR)";

    auto a = analyze(input);

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J");
    }
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, list_1_elem_text)
{
    std::string input = R"(
         GBLC &STR
         MAC_LIST_1_ELEM ()"
        + GetParam().name + "'J')";

    auto a = analyze(input);

    if (GetParam().is_consuming)
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
    else
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J'");
    }
}

TEST_P(parser_attribute_fixture, list_1_elem_number)
{
    std::string input = R"(
        GBLC &STR
        MAC_LIST_1_ELEM ()"
        + GetParam().name + "'9')";

    auto a = analyze(input);

    EXPECT_TRUE(a->diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'9'");
}

TEST_P(parser_attribute_fixture, list_1_elem_var_number_variants)
{
    std::string input = R"(
&VAR     SETC '9'
&NEG     SETC '-9'
         GBLC &STR
         MAC_LIST_1_ELEM ()"
        + GetParam().name + R"('&VAR)
         MAC_LIST_1_ELEM ()"
        + GetParam().name + R"('&NEG)
)";

    auto a = analyze(input);

    if (GetParam().is_consuming)
        EXPECT_TRUE(
            matches_message_codes(a->diags(), { "S0005", "S0005" })); // todo error should be "unbalanced parentheses"
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005", "S0005" }));
}

TEST_P(parser_attribute_fixture, list_1_elem_text_missing_apostrophe)
{
    std::string input = R"(
         GBLC &STR
         MAC_LIST_1_ELEM ()"
        + GetParam().name + "'J)";

    auto a = analyze(input);

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), GetParam().name + "'J");
    }
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, list_1_elem_number_missing_apostrophe)
{
    std::string input = R"(
         GBLC &STR
         MAC_LIST_1_ELEM ()"
        + GetParam().name + "'9)";

    auto a = analyze(input);

    EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, list_1_elem_var_instr_end_apostrophe_01)
{
    std::string input = R"(
&VAR     SETC 'J'
         GBLC &STR
         MAC_LIST_1_ELEM ()"
        + GetParam().name + "'&VAR')'";

    auto a = analyze(input);

    if (!GetParam().is_consuming)
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
    // else
    //     EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" })); // todo
}

TEST_P(parser_attribute_fixture, list_1_elem_var_number_end_apostrophe_01)
{
    std::string input = R"(
&VAR     SETC '9'
         GBLC &STR
         MAC_LIST_1_ELEM ()"
        + GetParam().name + "'&VAR')'";

    auto a = analyze(input);

    if (!GetParam().is_consuming)
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
    // else
    //{
    //     EXPECT_TRUE(a->diags().empty());
    //     EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), "(" + GetParam().name + "'9')'"); // todo
    // }
}

TEST_P(parser_attribute_fixture, list_1_elem_var_instr_end_apostrophe_02)
{
    std::string input = R"(
&VAR     SETC 'J'
         GBLC &STR
         MAC_LIST_1_ELEM ()"
        + GetParam().name + "'&VAR')''";

    auto a = analyze(input);

    EXPECT_TRUE(a->diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), "(" + GetParam().name + "'J')''");
}

TEST_P(parser_attribute_fixture, list_1_elem_var_number_end_apostrophe_02)
{
    std::string input = R"(
&VAR     SETC '9'
         GBLC &STR
         MAC_LIST_1_ELEM ()"
        + GetParam().name + "'&VAR')''";

    auto a = analyze(input);

    EXPECT_TRUE(a->diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), "(" + GetParam().name + "'9')''");
}

TEST_P(parser_attribute_fixture, list_2_elem_var_instr)
{
    std::string input = R"(
&VAR     SETC 'J'
         GBLC &STR1,&STR2
         MAC_LIST_2_ELEM (A,)"
        + GetParam().name + "'&VAR)";

    auto a = analyze(input);

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR1"), "A");
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR2"), GetParam().name + "'J");
    }
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, list_2_elem_text)
{
    std::string input = R"(
         GBLC &STR1,&STR2
         MAC_LIST_2_ELEM (A,)"
        + GetParam().name + "'J')";

    auto a = analyze(input);

    if (GetParam().is_consuming)
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
    else
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR1"), "A");
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR2"), GetParam().name + "'J'");
    }
}

TEST_P(parser_attribute_fixture, list_2_elem_var_number_variants)
{
    std::string input = R"(
&VAR     SETC '9'
&NEG     SETC '-9'
         GBLC &STR1,&STR2
         MAC_LIST_2_ELEM (A,)"
        + GetParam().name + R"('&VAR)
         MAC_LIST_2_ELEM (A,)"
        + GetParam().name + R"('&NEG)
)";

    auto a = analyze(input);

    if (GetParam().is_consuming)
        EXPECT_TRUE(
            matches_message_codes(a->diags(), { "S0005", "S0005" })); // todo error should be "unbalanced parentheses
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005", "S0005" }));
}

TEST_P(parser_attribute_fixture, list_2_elem_number)
{
    std::string input = R"(
         GBLC &STR1,&STR2
         MAC_LIST_2_ELEM (A,)"
        + GetParam().name + "'9')";

    auto a = analyze(input);

    EXPECT_TRUE(a->diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR1"), "A");
    EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR2"), GetParam().name + "'9'");
}

TEST_P(parser_attribute_fixture, list_2_elem_text_missing_apostrophe)
{
    std::string input = R"(
         GBLC &STR1,&STR2
         MAC_LIST_2_ELEM (A,)"
        + GetParam().name + "'J)";

    auto a = analyze(input);

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR1"), "A");
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR2"), GetParam().name + "'J");
    }
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, list_2_elem_number_variants_missing_apostrophe)
{
    std::string input = R"(
         GBLC &STR1,&STR2
         MAC_LIST_2_ELEM (A,)"
        + GetParam().name + R"('9)
         MAC_LIST_2_ELEM (A,)"
        + GetParam().name + R"('-9)
)";

    auto a = analyze(input);

    EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005", "S0005" }));
}

TEST_P(parser_attribute_fixture, no_ending_apostrophe)
{
    std::string input = R"(
         GBLC &STR
         MAC ")"
        + GetParam().name + "'SYM";

    auto a = analyze(input);

    if (GetParam().is_consuming)
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), "\"" + GetParam().name + "'SYM");
    }
    else
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
}

TEST_P(parser_attribute_fixture, no_ending_apostrophe_2)
{
    std::string input = R"(
         GBLC &STR
         MAC ")"
        + GetParam().name + "'SYM' STH";

    auto a = analyze(input);

    if (GetParam().is_consuming)
        EXPECT_TRUE(matches_message_codes(a->diags(), { "S0005" }));
    else
    {
        EXPECT_TRUE(a->diags().empty());
        EXPECT_EQ(get_var_value<C_t>(a->hlasm_ctx(), "STR"), "\"" + GetParam().name + "'SYM'");
    }
}

TEST_P(parser_attribute_fixture, preserve_structured_parameter)
{
    std::string input = R"(
      GBLC  &PAR1,&PAR2
      MACRO
      MAC2
      GBLC  &PAR1,&PAR2
&PAR1 SETC  '&SYSLIST(1,1)'
&PAR2 SETC  '&SYSLIST(1,2)'
      MEND

      MACRO
      MAC   &P1
      MAC2  &P1
      MEND

      MAC   (A,)"
        + GetParam().name + "'-9')";

    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(a.diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "PAR1"), "A");
    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "PAR2"), GetParam().name + "'-9'");
}

TEST_P(parser_attribute_fixture, preserve_structured_parameter_2)
{
    std::string input = R"(
      GBLC  &PAR1,&PAR2
      MACRO
      MAC2
      GBLC  &PAR1,&PAR2
&PAR1 SETC  '&SYSLIST(1,1)'
&PAR2 SETC  '&SYSLIST(1,2)'
      MEND

      MACRO
      MAC   &P1
      MAC2  &P1.
      MEND

      MAC   (A,)"
        + GetParam().name + "'-9')";

    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(a.diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "PAR1"), "A");
    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "PAR2"), GetParam().name + "'-9'");
}

TEST(parser, preconstructed_string)
{
    std::string input = R"(
         MACRO
         MAC2
         MEND

         MACRO
         MAC &PAR
&HASH    SETC  'I''#RULE'
&NUM     SETC  'I''1RULE'
&NEG     SETC  'I''-1RULE'
&EQ      SETC  'I''=RULE'
&CHAR    SETC  'I''RULE'
&PAR2    SETC  'I''&PAR'
         MAC2  (&HASH)
         MAC2  (&NUM)
         MAC2  (&NEG)
         MAC2  (&EQ)
         MAC2  (&CHAR)
         MAC2  (&PAR2)
         MEND

         MAC PARAMETER)";

    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(a.diags().empty());
}

TEST(parser, consuming_attribute)
{
    std::string input = R"(
         MACRO
         MAC2 &NAMELEN=,&PLIST=PLIST
         MEND

         MACRO
         MAC &PLIST=PLIST,&STGNAME='STG'

         MAC2 NAMELEN=L'=C&STGNAME.,                                   X
               DATA=24(R13),PLIST=&PLIST
         MEND

         MAC
         END)";

    analyzer a(input);
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(a.diags().empty());
}