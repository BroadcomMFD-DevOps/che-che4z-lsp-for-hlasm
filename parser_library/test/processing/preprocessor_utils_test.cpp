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

namespace {
static constexpr const lexing::logical_line_extractor_args extract_opts { 1, 71, 2, false, false };

range get_range(size_t text_length) { return range(position(0, 0), position(0, text_length)); }

std::string get_inline_string(std::string_view text, const lexing::logical_line_extractor_args& opts)
{
    lexing::logical_line out;
    out.clear();

    bool feed = true;
    while (feed)
        feed = append_to_logical_line(out, text, opts);

    finish_logical_line(out, opts);

    return std::string(out.begin(), out.end());
}
} // namespace

TEST(preprocessor_utils, operand_parsing_single)
{
    std::string input = get_inline_string(R"(  ABCODE    )", extract_opts);

    std::vector<semantics::preproc_details::name_range> expected {
        { "ABCODE", range(position(0, 2), position(0, 8)) },
    };

    EXPECT_EQ(processing::get_operands_list(input, get_range(input.length()), 1), expected);
}

TEST(preprocessor_utils, operand_parsing_single_argument)
{
    std::string input = get_inline_string(R"(ABCODE('1234')   )", extract_opts);

    std::vector<semantics::preproc_details::name_range> expected {
        { "ABCODE('1234')", range(position(0, 0), position(0, 14)) },
    };

    EXPECT_EQ(processing::get_operands_list(input, get_range(input.length()), 1), expected);
}

TEST(preprocessor_utils, operand_parsing_single_argument_multiline)
{
    std::string input = get_inline_string(R"(ABCODE('12                                                             X
        34' ))",
        extract_opts);

    std::vector<semantics::preproc_details::name_range> expected {
        { "ABCODE('1234')", range(position(0, 0), position(1, 13)) },
    };

    EXPECT_EQ(processing::get_operands_list(input, get_range(input.length()), 1), expected);
}

TEST(preprocessor_utils, operand_parsing_multiple)
{
    std::string input = get_inline_string(R"(ABCODE ( '1234' ) NODUMP RECFM ( X'02' ) OPERAND ('4321'))", extract_opts);

    std::vector<semantics::preproc_details::name_range> expected {
        { "ABCODE('1234')", range(position(0, 0), position(0, 17)) },
        { "NODUMP", range(position(0, 18), position(0, 24)) },
        { "RECFM(X'02')", range(position(0, 25), position(0, 40)) },
        { "OPERAND('4321')", range(position(0, 41), position(0, 57)) },
    };

    EXPECT_EQ(processing::get_operands_list(input, get_range(input.length()), 1), expected);
}

TEST(preprocessor_utils, operand_parsing_multiple_comma_separated)
{
    std::string input = get_inline_string(R"(1,2,3,DFHVALUE(ACQUIRED))", extract_opts);

    std::vector<semantics::preproc_details::name_range> expected {
        { "1", range(position(0, 0), position(0, 1)) },
        { "2", range(position(0, 2), position(0, 3)) },
        { "3", range(position(0, 4), position(0, 5)) },
        { "DFHVALUE(ACQUIRED)", range(position(0, 6), position(0, 24)) },
    };

    EXPECT_EQ(processing::get_operands_list(input, get_range(input.length()), 1), expected);
}

TEST(preprocessor_utils, operand_parsing_multiple_multiline)
{
    auto input = get_inline_string(
        R"(ABCODE ( '1234' )                                                      X
              NODUMP                                                   X
              OPERAND ('4321'))",
        extract_opts);

    std::vector<semantics::preproc_details::name_range> expected {
        { "ABCODE('1234')", range(position(0, 0), position(0, 17)) },
        { "NODUMP", range(position(1, 14), position(1, 20)) },
        { "OPERAND('4321')", range(position(2, 14), position(2, 30)) },
    };

    EXPECT_EQ(processing::get_operands_list(input, get_range(input.length()), 1), expected);
}