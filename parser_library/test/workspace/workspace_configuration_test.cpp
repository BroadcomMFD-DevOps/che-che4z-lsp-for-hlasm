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
#include "workspaces/workspace_configuration.h"

using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::parser_library::workspaces;

namespace {
template<int>
struct X
{
    char a;

    auto operator<=>(const X&) const = default;
};
} // namespace

TEST(workspace_configuration, library_options)
{
    constexpr auto eq = [](auto&& l, auto&& r) { return !(l < r || r < l); };

    X<0> x0_0 { 0 };
    library_options lx0_1(X<0> { 0 });
    EXPECT_TRUE(eq(lx0_1, x0_0));

    library_options lx0_2(X<0> { 0 });
    EXPECT_TRUE(eq(lx0_1, lx0_2));

    library_options lx0_moved_1(std::move(lx0_1));
    library_options lx0_moved_2(std::move(lx0_2));
    EXPECT_TRUE(eq(lx0_moved_1, lx0_moved_2));
    EXPECT_TRUE(eq(lx0_1, lx0_2));
    EXPECT_FALSE(eq(lx0_1, lx0_moved_1));

    X<0> x0_1 { 1 };
    EXPECT_TRUE(lx0_moved_1 < x0_1);
    EXPECT_TRUE(lx0_moved_1 < library_options(x0_1));

    X<1> x1_0 { 0 };
    EXPECT_FALSE(eq(lx0_moved_1, x1_0));
    EXPECT_FALSE(eq(lx0_moved_1, library_options(x1_0)));

    library_options lx1_1(x1_0);
    library_options lx1_2(x1_0);
    library_options lx1_3(x0_0);

    EXPECT_TRUE(eq(lx1_1, lx1_2));
    EXPECT_FALSE(eq(lx1_2, lx1_3));

    lx1_3 = library_options(x1_0);
    EXPECT_TRUE(eq(lx1_1, lx1_3));

    EXPECT_TRUE(library_options(X<2> { 1 }) < X<2> { 2 });
    EXPECT_TRUE(library_options(X<2> { 1 }) < library_options(X<2> { 2 }));
}
