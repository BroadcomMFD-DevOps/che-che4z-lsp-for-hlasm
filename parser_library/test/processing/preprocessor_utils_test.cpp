/*
 * Copyright (c) 2022 Broadcom.
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
#include "processing/preprocessors/preprocessor_utils.h"
#include "semantics/statement.h"

TEST(preprocessor_utils, operand_parsing_single)
{
    const size_t lineno = 0;
    std::string input(R"(  ABCODE    )");

    auto rp = semantics::range_provider(
        range({ lineno, 0 }, { lineno, input.length() }), semantics::adjusting_state::MACRO_REPARSE, 0);

    auto ops = processing::get_operands_list(input, 0, lineno, rp);

    std::vector<semantics::preproc_details::name_range> expected {
        { "ABCODE", range(position(0, 2), position(0, 8)) },
    };

    EXPECT_EQ(ops, expected);
}

TEST(preprocessor_utils, operand_parsing_single_argument)
{
    const size_t lineno = 0;
    std::string input(R"(ABCODE('1234')   )");

    auto rp = semantics::range_provider(
        range({ lineno, 0 }, { lineno, input.length() }), semantics::adjusting_state::MACRO_REPARSE, 0);

    auto ops = processing::get_operands_list(input, 0, lineno, rp);

    std::vector<semantics::preproc_details::name_range> expected {
        { "ABCODE('1234')", range(position(0, 0), position(0, 14)) },
    };

    EXPECT_EQ(ops, expected);
}

TEST(preprocessor_utils, operand_parsing_single_argument_multiline)
{
    const size_t lineno = 0;
    std::string input(R"(ABCODE('12                                                                     34' ))");

    auto rp = semantics::range_provider(
        range({ lineno, 0 }, { lineno, input.length() }), semantics::adjusting_state::MACRO_REPARSE, 0);

    auto ops = processing::get_operands_list(input, 0, lineno, rp);

    std::vector<semantics::preproc_details::name_range> expected {
        { "ABCODE('1234')", range(position(0, 0), position(1, 13)) },
    };

    EXPECT_EQ(ops, expected);
}

TEST(preprocessor_utils, operand_parsing_multiple)
{
    const size_t lineno = 0;
    std::string input(R"(ABCODE ( '1234' ) NODUMP RECFM ( X'02' ) OPERAND ('4321'))");

    auto rp = semantics::range_provider(
        range({ lineno, 0 }, { lineno, input.length() }), semantics::adjusting_state::MACRO_REPARSE, 0);

    auto ops = processing::get_operands_list(input, 0, lineno, rp);

    std::vector<semantics::preproc_details::name_range> expected {
        { "ABCODE('1234')", range(position(0, 0), position(0, 17)) },
        { "NODUMP", range(position(0, 18), position(0, 24)) },
        { "RECFM(X'02')", range(position(0, 25), position(0, 40)) },
        { "OPERAND('4321')", range(position(0, 41), position(0, 57)) },
    };

    EXPECT_EQ(ops, expected);
}

TEST(preprocessor_utils, operand_parsing_multiple_comma_separated)
{
    const size_t lineno = 0;
    std::string input(R"(1,2,3,DFHVALUE(ACQUIRED))");

    auto rp = semantics::range_provider(
        range({ lineno, 0 }, { lineno, input.length() }), semantics::adjusting_state::MACRO_REPARSE, 0);

    auto ops = processing::get_operands_list(input, 0, lineno, rp);

    std::vector<semantics::preproc_details::name_range> expected {
        { "1", range(position(0, 0), position(0, 1)) },
        { "2", range(position(0, 2), position(0, 3)) },
        { "3", range(position(0, 4), position(0, 5)) },
        { "DFHVALUE(ACQUIRED)", range(position(0, 6), position(0, 24)) },
    };

    EXPECT_EQ(ops, expected);
}

TEST(preprocessor_utils, operand_parsing_multiple_multiline)
{
    const size_t lineno = 0;
    std::string input(
        R"(ABCODE ( '1234' )                                                                    NODUMP                                                                 OPERAND ('4321'))");

    auto rp = semantics::range_provider(
        range({ lineno, 0 }, { lineno, input.length() }), semantics::adjusting_state::MACRO_REPARSE, 0);

    auto ops = processing::get_operands_list(input, 0, lineno, rp);

    std::vector<semantics::preproc_details::name_range> expected {
        { "ABCODE('1234')", range(position(0, 0), position(0, 17)) },
        { "NODUMP", range(position(1, 14), position(1, 20)) },
        { "OPERAND('4321')", range(position(2, 14), position(2, 30)) },
    };

    EXPECT_EQ(ops, expected);
}