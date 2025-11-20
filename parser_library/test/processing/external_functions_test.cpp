/*
 * Copyright (c) 2025 Broadcom.
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

TEST(external_functions, arithmetic)
{
    std::string input(R"(
&A  SETAF 'ADD',1,2
)");
    analyzer a(input,
        analyzer_options(external_functions_list {
            {
                "ADD",
                [](external_function_args& args) {
                    auto* aarg = args.arithmetic();
                    if (!aarg)
                        return;
                    for (auto x : aarg->args)
                        aarg->result += x;
                },
            },
        }));
    a.analyze();

    EXPECT_TRUE(a.diags().empty());

    auto& ctx = a.hlasm_ctx();

    EXPECT_EQ(get_var_value<A_t>(ctx, "A"), 3);
}

TEST(external_functions, character)
{
    std::string input(R"(
&A  SETCF 'ADD','A','B'
)");
    analyzer a(input,
        analyzer_options(external_functions_list {
            {
                "ADD",
                [](external_function_args& arg) {
                    auto* carg = arg.character();
                    if (!carg)
                        return;
                    for (auto x : carg->args)
                        carg->result += x;
                },
            },
        }));
    a.analyze();

    EXPECT_TRUE(a.diags().empty());

    auto& ctx = a.hlasm_ctx();

    EXPECT_EQ(get_var_value<C_t>(ctx, "A"), "AB");
}
