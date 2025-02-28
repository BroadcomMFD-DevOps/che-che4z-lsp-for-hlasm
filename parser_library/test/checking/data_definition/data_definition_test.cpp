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

#include "../../common_testing.h"
#include "context/hlasm_context.h"
#include "context/ordinary_assembly/ordinary_assembly_dependency_solver.h"
#include "expressions/data_definition.h"
#include "library_info_transitional.h"

void expect_no_errors(const std::string& text)
{
    std::string input = (text);
    analyzer a(input);
    a.analyze();

    EXPECT_TRUE(a.diags().empty());
}

void expect_errors(const std::string& text, std::initializer_list<std::string> msgs = {})
{
    std::string input = (text);
    analyzer a(input);
    a.analyze();

    EXPECT_FALSE(a.diags().empty());
    EXPECT_TRUE(contains_message_codes(a.diags(), msgs));
}

TEST(data_definition_grammar, modifiers)
{
    expect_no_errors(R"( DC 10FDP(123)L(2*3)S(2*4)E(-12*2)'2.25'
 DC 10FDP(123)L2S(2*4)E(-12*2)'2.25'
 DC 10FDP(123)L(2*3)S6E(-12*2)'2.25'
 DC 10FDP(123)L.(2*3)S6E(-12*2)'2.25'
 DC 10FDP(123)L(2*3)S(2*4)E12'2.25'
 DC 10FDP(123)L(2*3)S(2*4)E-12'2.25'
 DC 10FDP(123)L(2*3)S6E0'2.25'
 DC 10FDP(123)L.(2*3)S6E0'2.25'
 DC 10FDP(123)L3S(2*4)E12'2.25'
 DC 10FDP(123)L1S30E(-12*2)'2.25'
 DC 10FDP(123)L1S-30E(-12*2)'2.25'
 DC 10FDP(123)L.1S30E(-12*2)'2.25'
 DC 10FDP(123)L1S30E40'2.25'
 DC 10FDP(123)L1S-30E-40'2.25'
 DC 10FDP(123)L.1S30E40'2.25'
 DC 10FDP(123)L.1S-30E-40'2.25'

 DC (1*8)FDP(123)L2S(2*4)E(-12*2)'2.25'
 DC (1*8)FDP(123)L(2*3)S6E(-12*2)'2.25'
 DC (1*8)FDP(123)L(2*3)S(2*4)E12'2.25'
 DC (1*8)FDP(123)L(2*3)S6E0'2.25'
 DC (1*8)FDP(123)L3S(2*4)E12'2.25'
 DC (1*8)FDP(123)L1S30E(-12*2)'2.25'
 DC (1*8)FDP(123)L1S30E40'2.25'
 DC EE(1)'1'

 DC 10FDL(2*3)S(2*4)E(-12*2)'2.25'
 DC 10FDL2S(2*4)E(-12*2)'2.25'
 DC 10FDL(2*3)S6E(-12*2)'2.25'
 DC 10FDL(2*3)S(2*4)E12'2.25'
 DC 10FDL(2*3)S6E0'2.25'
 DC 10FDL3S(2*4)E12'2.25'
 DC 10FDL1S30E(-12*2)'2.25'
 DC 10FDL1S30E40'2.25'

 DC (1*8)FDL(2*3)S(2*4)E(-12*2)'2.25'
 DC (1*8)FDL2S(2*4)E(-12*2)'2.25'
 DC (1*8)FDL(2*3)S6E(-12*2)'2.25'
 DC (1*8)FDL(2*3)S(2*4)E12'2.25'
 DC (1*8)FDL(2*3)S6E0'2.25'
 DC (1*8)FDL3S(2*4)E12'2.25'
 DC (1*8)FDL1S30E(-12*2)'2.25'
 DC (1*8)FDL1S30E40'2.25'
 DC 13FL.(13)'2.25')");

    expect_errors(" DC 10FDP(123)L(2*3)S(2*4)E(-12*2)(34)'2.25'");
    expect_errors(" DC 10FDP(123)(1)L(2*3)S(2*4)E(-12*2)'2.25'");
    expect_errors(" DC (1*8)FDL1S(1+2)(3+1)E40'2.25'");
    expect_errors(" DC %");
}

TEST(data_definition_grammar, modifiers_lower_case)
{
    expect_no_errors(R"(
 dc 10fdp(123)l(2*3)s(2*4)e(-12*2)'2.25'
 dc 10fdp(123)l2s(2*4)e(-12*2)'2.25'
 dc 10fdp(123)l(2*3)s6e(-12*2)'2.25'
 dc 10fdp(123)l.(2*3)s6e(-12*2)'2.25'
 dc 10fdp(123)l(2*3)s(2*4)e12'2.25'
 dc 10fdp(123)l(2*3)s(2*4)e-12'2.25'
 dc 10fdp(123)l(2*3)s6e0'2.25'
 dc 10fdp(123)l.(2*3)s6e0'2.25'
 dc 10fdp(123)l3s(2*4)e12'2.25'
 dc 10fdp(123)l1s30e(-12*2)'2.25'
 dc 10fdp(123)l1s-30e(-12*2)'2.25'
 dc 10fdp(123)l.1s30e(-12*2)'2.25'
 dc 10fdp(123)l1s30e40'2.25'
 dc 10fdp(123)l1s-30e-40'2.25'
 dc 10fdp(123)l.1s30e40'2.25'
 dc 10fdp(123)l.1s-30e-40'2.25'

 dc (1*8)fdp(123)l2s(2*4)e(-12*2)'2.25'
 dc (1*8)fdp(123)l(2*3)s6e(-12*2)'2.25'
 dc (1*8)fdp(123)l(2*3)s(2*4)e12'2.25'
 dc (1*8)fdp(123)l(2*3)s6e0'2.25'
 dc (1*8)fdp(123)l3s(2*4)e12'2.25'
 dc (1*8)fdp(123)l1s30e(-12*2)'2.25'
 dc (1*8)fdp(123)l1s30e40'2.25'
 dc ee(1)'1'

 dc 10fdl(2*3)s(2*4)e(-12*2)'2.25'
 dc 10fdl2s(2*4)e(-12*2)'2.25'
 dc 10fdl(2*3)s6e(-12*2)'2.25'
 dc 10fdl(2*3)s(2*4)e12'2.25'
 dc 10fdl(2*3)s6e0'2.25'
 dc 10fdl3s(2*4)e12'2.25'
 dc 10fdl1s30e(-12*2)'2.25'
 dc 10fdl1s30e40'2.25'

 dc (1*8)fdl(2*3)s(2*4)e(-12*2)'2.25'
 dc (1*8)fdl2s(2*4)e(-12*2)'2.25'
 dc (1*8)fdl(2*3)s6e(-12*2)'2.25'
 dc (1*8)fdl(2*3)s(2*4)e12'2.25'
 dc (1*8)fdl(2*3)s6e0'2.25'
 dc (1*8)fdl3s(2*4)e12'2.25'
 dc (1*8)fdl1s30e(-12*2)'2.25'
 dc (1*8)fdl1s30e40'2.25'
 dc 13fl.(13)'2.25'
)");
}

TEST(data_definition_grammar, address_nominal)
{
    expect_no_errors(" DC (1*8)S(512(12))");
    expect_no_errors(" DC 8S(512(12))");
    expect_no_errors(" DC S(512(12))");
    expect_no_errors(" DC SP(13)(512(12))");
    expect_no_errors(" DC SP(13)L2(512(12))");
    expect_no_errors(" DC SP(13)L(2)(512(12))");
    expect_no_errors(" DC S(512(12),418(0))");
    expect_no_errors(
        R"(  USING A,5
     DC S(512(12),418(0),A_field)
A       DSECT
A_field DS F)");
    expect_no_errors(" DC S(512(0))");
    expect_no_errors("A DC S(*-A+4(0))");

    expect_errors(" DC S(512())");
    expect_errors(" DC S(512(0)");
    expect_errors(" DC SP(13)L(13)(512(12,13))");
    expect_errors(" DC A(512(12)");
}

TEST(data_definition_grammar, expression_nominal)
{
    expect_no_errors("A DC A(*-A,*+4)");
    expect_no_errors("A DC A(A+32)");
    expect_no_errors("A DC AL4(A+32)");
    expect_no_errors("A DC AL(4)(A+32)");
    expect_no_errors("A DC 10AL(4)(A+32)");
    expect_no_errors("A DC (1+9)A(*-A,*+4)");
}

TEST(data_definition_grammar, no_nominal)
{
    expect_no_errors("A DC 0C");
    expect_no_errors("A DC 0CL10");
    expect_no_errors("A DC 0CL(1+10)");
}

TEST(data_definition, duplication_factor)
{
    std::string input = "13C'A'";
    analyzer a(input);
    diagnostic_op_consumer_container diags;

    auto parsed = parse_data_definition(a, &diags);

    EXPECT_TRUE(diags.diags.empty());

    context::ordinary_assembly_dependency_solver dep_solver(a.hlasm_ctx().ord_ctx, library_info_transitional::empty);
    diags.diags.clear();

    auto dup_f = parsed.dupl_factor->evaluate(dep_solver, diags).get_abs();
    EXPECT_EQ(dup_f, 13);
}

TEST(data_definition, duplication_factor_expr)
{
    std::string input = "(13*2)C'A'";
    analyzer a(input);
    diagnostic_op_consumer_container diags;

    auto parsed = parse_data_definition(a, &diags);

    EXPECT_TRUE(diags.diags.empty());

    context::ordinary_assembly_dependency_solver dep_solver(a.hlasm_ctx().ord_ctx, library_info_transitional::empty);
    diags.diags.clear();

    auto dup_f = parsed.dupl_factor->evaluate(dep_solver, diags).get_abs();
    EXPECT_EQ(dup_f, 26);
}

TEST(data_definition, duplication_factor_out_of_range)
{
    std::string input = "1231312123123123123C'A'";
    analyzer a(input);
    diagnostic_op_consumer_container diags;

    auto parsed = parse_data_definition(a, &diags);

    EXPECT_FALSE(diags.diags.empty());
}

TEST(data_definition, duplication_factor_invalid_number)
{
    std::string input = "-C'A'";
    analyzer a(input);
    diagnostic_op_consumer_container diags;

    auto parsed = parse_data_definition(a, &diags);

    EXPECT_FALSE(diags.diags.empty());
}

TEST(data_definition, all_fields)
{
    std::string input = "(1*8)FDP(123)L2S(2*4)E(-12*2)'2.25'";
    analyzer a(input);
    diagnostic_op_consumer_container diags;

    auto parsed = parse_data_definition(a, &diags);

    EXPECT_TRUE(diags.diags.empty());

    context::ordinary_assembly_dependency_solver dep_solver(a.hlasm_ctx().ord_ctx, library_info_transitional::empty);
    diags.diags.clear();

    auto dup_f = parsed.dupl_factor->evaluate(dep_solver, diags).get_abs();
    EXPECT_EQ(dup_f, 8);

    EXPECT_EQ(parsed.program_type->evaluate(dep_solver, diags).get_abs(), 123);
    EXPECT_EQ(parsed.length->evaluate(dep_solver, diags).get_abs(), 2);
    EXPECT_EQ(parsed.length_type, expressions::data_definition::length_type::BYTE);
    EXPECT_EQ(parsed.scale->evaluate(dep_solver, diags).get_abs(), 8);
    EXPECT_EQ(parsed.exponent->evaluate(dep_solver, diags).get_abs(), -24);
    ASSERT_NE(parsed.nominal_value->access_string(), nullptr);
    EXPECT_EQ(parsed.nominal_value->access_string()->value, "2.25");
}

TEST(data_definition, no_nominal)
{
    std::string input = "0FDL2";
    analyzer a(input);
    diagnostic_op_consumer_container diags;

    auto parsed = parse_data_definition(a, &diags);

    EXPECT_TRUE(diags.diags.empty());

    context::ordinary_assembly_dependency_solver dep_solver(a.hlasm_ctx().ord_ctx, library_info_transitional::empty);
    diags.diags.clear();

    EXPECT_EQ(parsed.dupl_factor->evaluate(dep_solver, diags).get_abs(), 0);
    EXPECT_EQ(parsed.program_type, nullptr);
    EXPECT_EQ(parsed.length->evaluate(dep_solver, diags).get_abs(), 2);
    EXPECT_EQ(parsed.length_type, expressions::data_definition::length_type::BYTE);
    EXPECT_EQ(parsed.scale, nullptr);
    EXPECT_EQ(parsed.exponent, nullptr);
    ASSERT_EQ(parsed.nominal_value, nullptr);
}

TEST(data_definition, no_nominal_expr)
{
    std::string input = "0FDL(2+2)";
    analyzer a(input);
    diagnostic_op_consumer_container diags;

    auto parsed = parse_data_definition(a, &diags);

    EXPECT_TRUE(diags.diags.empty());

    context::ordinary_assembly_dependency_solver dep_solver(a.hlasm_ctx().ord_ctx, library_info_transitional::empty);
    diags.diags.clear();

    EXPECT_EQ(parsed.dupl_factor->evaluate(dep_solver, diags).get_abs(), 0);
    EXPECT_EQ(parsed.program_type, nullptr);
    EXPECT_EQ(parsed.length->evaluate(dep_solver, diags).get_abs(), 4);
    EXPECT_EQ(parsed.length_type, expressions::data_definition::length_type::BYTE);
    EXPECT_EQ(parsed.scale, nullptr);
    EXPECT_EQ(parsed.exponent, nullptr);
    ASSERT_EQ(parsed.nominal_value, nullptr);
}

TEST(data_definition, bit_length)
{
    std::string input = "(1*8)FDP(123)L.2S-8E(-12*2)'2.25'";
    analyzer a(input);
    diagnostic_op_consumer_container diags;

    auto parsed = parse_data_definition(a, &diags);

    EXPECT_TRUE(diags.diags.empty());

    context::ordinary_assembly_dependency_solver dep_solver(a.hlasm_ctx().ord_ctx, library_info_transitional::empty);
    diags.diags.clear();

    EXPECT_EQ(parsed.dupl_factor->evaluate(dep_solver, diags).get_abs(), 8);

    EXPECT_EQ(parsed.program_type->evaluate(dep_solver, diags).get_abs(), 123);
    EXPECT_EQ(parsed.length->evaluate(dep_solver, diags).get_abs(), 2);
    EXPECT_EQ(parsed.length_type, expressions::data_definition::length_type::BIT);
    EXPECT_EQ(parsed.scale->evaluate(dep_solver, diags).get_abs(), -8);
    EXPECT_EQ(parsed.exponent->evaluate(dep_solver, diags).get_abs(), -24);
    ASSERT_NE(parsed.nominal_value->access_string(), nullptr);
    EXPECT_EQ(parsed.nominal_value->access_string()->value, "2.25");
}

TEST(data_definition, unexpected_dot)
{
    std::string input = "(1*8)FDL.2S.-8E(-12*2)'2.25'";
    analyzer a(input);
    diagnostic_op_consumer_container diags;

    auto parsed = parse_data_definition(a, &diags);

    EXPECT_FALSE(diags.diags.empty());
}

TEST(data_definition, unexpected_minus)
{
    std::string input = "(1*8)FDL.2S.-E(-12*2)'2.25'";
    analyzer a(input);
    diagnostic_op_consumer_container diags;

    auto parsed = parse_data_definition(a, &diags);

    EXPECT_FALSE(diags.diags.empty());
}

TEST(data_definition, wrong_modifier_order)
{
    std::string input = "1HL-12P(123)S1'1.25'";
    analyzer a(input);
    diagnostic_op_consumer_container diags;

    auto parsed = parse_data_definition(a, &diags);

    EXPECT_FALSE(diags.diags.empty());
}

TEST(data_definition, B_wrong_nominal_value)
{
    std::string input = " DC B'12'";
    analyzer a(input);
    a.analyze();

    EXPECT_EQ(a.diags().size(), (size_t)1);
}

TEST(data_definition, suppres_syntax_errors_in_macro)
{
    std::string input = R"(
    MACRO
    MAC
    DC AL(0)
    MEND
)";
    analyzer a(input);
    a.analyze();

    EXPECT_TRUE(a.diags().empty());
}

TEST(data_definition, syntax_error_for_each_call)
{
    std::string input = R"(
    MACRO
    MAC
    DS AL
    MEND

    MAC
    MAC
)";
    analyzer a(input);
    a.analyze();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "S0003", "S0003", "A010", "A010" }));
}

TEST(data_definition, trim_labels)
{
    std::string input = R"(
&L SETC 'LABEL '
&L EQU  0
)";

    analyzer a(input);
    a.analyze();

    EXPECT_TRUE(a.diags().empty());
}

TEST(data_definition, externals)
{
    expect_no_errors(" EXTRN E1,E2\n DC A(E1,E2)");
    expect_no_errors(" WXTRN W1,W2\n DC A(W1,W2)");
}

TEST(data_definition, duplicate_externals)
{
    expect_errors(" EXTRN E1\n EXTRN E1", { "E031" });
    expect_errors(" WXTRN W1\n WXTRN W1", { "E031" });
}

TEST(data_definition, externals_sequence_support)
{
    expect_no_errors(" AGO .ABC\n.ABC EXTRN E1");
    expect_no_errors(" AGO .ABC\n.ABC WXTRN W1");

    expect_errors("ABC EXTRN E1", { "A249" });
    expect_errors("ABC WXTRN W1", { "A249" });
}

TEST(data_definition, externals_no_expressions)
{
    expect_errors(" EXTRN E1+2");
    expect_errors(" WXTRN W1+2");
    expect_errors(" EXTRN PART(E1+2)");
    expect_errors(" WXTRN PART(W1+2)");
}

TEST(data_definition, externals_type_support)
{
    std::string input = R"(
     WXTRN A
     EXTRN B
&A   SETC T'A
&B   SETC T'B
)";

    analyzer a(input);
    a.analyze();

    EXPECT_TRUE(a.diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "A"), "$");
    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "B"), "T");
}

TEST(data_definition, externals_part_type_support)
{
    std::string input = R"(
     WXTRN PART(A)
     EXTRN PART(B)
&A   SETC T'A
&B   SETC T'B
)";

    analyzer a(input);
    a.analyze();

    EXPECT_TRUE(a.diags().empty());
    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "A"), "$");
    EXPECT_EQ(get_var_value<C_t>(a.hlasm_ctx(), "B"), "T");
}

TEST(data_definition, externals_part_support)
{
    expect_no_errors(" EXTRN PART(E1)");
    expect_no_errors(" WXTRN PART(W1)");
    expect_no_errors(" EXTRN PART(E1,E2)");
    expect_no_errors(" WXTRN PART(W1,W2)");
    expect_no_errors(" EXTRN PART(E1),PART(E2)");
    expect_no_errors(" WXTRN PART(W1),PART(W2)");

    expect_errors(" EXTRN PART(E1+1)");
    expect_errors(" WXTRN PART(W1+1)");
    expect_errors(" EXTRN PART()");
    expect_errors(" WXTRN PART()");
}

TEST(data_definition, moving_loctr)
{
    std::string input = R"(
X    DC  (*-X+1)XL(*-X+1)'0',(*-X+1)F'0'
LX   EQU *-X
TEST DS  0FD
     DS  A
B    DS  A
Y    DC  FL.(2*(*-TEST))'0',FL.(2*(*-TEST))'-1',FL.12'0'
LY   EQU *-Y
Z    DC  3FL(*-Z+1)'0',3FL(*-Z+1)'0'
LZ   EQU *-Z
)";
    analyzer a(input);
    a.analyze();
    EXPECT_TRUE(a.diags().empty());

    EXPECT_EQ(get_symbol_abs(a.hlasm_ctx(), "LX"), 24);
    EXPECT_EQ(get_symbol_abs(a.hlasm_ctx(), "LY"), 6);
    EXPECT_EQ(get_symbol_abs(a.hlasm_ctx(), "LZ"), 15);
}

TEST(data_definition, no_loctr_ref)
{
    std::string input = "(2*2)ADL(2*2)(2*2)";
    analyzer a(input);
    diagnostic_op_consumer_container diags;

    auto parsed = parse_data_definition(a, &diags);

    EXPECT_EQ(diags.diags.size(), (size_t)0);
    EXPECT_FALSE(parsed.references_loctr);
}

TEST(data_definition, loctr_ref)
{
    for (std::string input : { "(*-*)ADL(2*2)(2*2)", "(2*2)ADL(*-*)(2*2)", "(2*2)ADL(2*2)(*-*)" })
    {
        analyzer a(input);
        diagnostic_op_consumer_container diags;

        auto parsed = parse_data_definition(a, &diags);

        EXPECT_EQ(diags.diags.size(), (size_t)0);
        EXPECT_TRUE(parsed.references_loctr);
    }
}

TEST(data_definition, multivalue_alignment)
{
    std::string input = R"(
X   DSECT
    DS    H
    DS    H,3F
LEN EQU   *-X
)";
    analyzer a(input);
    a.analyze();

    EXPECT_TRUE(a.diags().empty());
    EXPECT_EQ(get_symbol_abs(a.hlasm_ctx(), "LEN"), 16);
}

TEST(data_definition, multivalue_alignment_misaligned)
{
    std::string input = R"(
X   DSECT
    DS    C
    DS    H,2FD
LEN EQU   *-X
)";
    analyzer a(input);
    a.analyze();

    EXPECT_TRUE(a.diags().empty());
    EXPECT_EQ(get_symbol_abs(a.hlasm_ctx(), "LEN"), 24);
}

TEST(data_definition, tolerate_qualifier)
{
    std::string input = R"(
C   CSECT
Q   USING C,1
    DC    A(Q.L-C)
L   EQU   *
)";
    analyzer a(input);
    a.analyze();

    EXPECT_TRUE(a.diags().empty());
}

TEST(data_definition, continued_nominal_value_in_macro)
{
    std::string input = R"(
     MACRO
     MAC
LEN  DS   CL120
X    DC   CL(L'LEN)'                                                   X
               AAA'
TEST EQU  *-X
     MEND

     MAC
)";
    analyzer a(input);
    a.analyze();

    EXPECT_TRUE(a.diags().empty());
    EXPECT_EQ(get_symbol_abs(a.hlasm_ctx(), "TEST"), 120);
}

TEST(data_definition, dependency_redefinition)
{
    std::string input = R"(
O2  DS  AL(O1)
O2  DS  AL(O1)
O1  EQU 1
)";
    analyzer a(input);
    a.analyze();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "E031" }));
}
