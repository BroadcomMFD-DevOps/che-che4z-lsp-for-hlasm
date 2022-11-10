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

#include "utils/bk_tree.h"
#include "utils/levenshtein_distance.h"

using namespace hlasm_plugin::utils;


TEST(bk_tree, simple_insert)
{
    const auto dist = [](long long a, long long b) { return (size_t)std::abs(a - b); };
    bk_tree<unsigned, decltype(dist)> tree;

    EXPECT_EQ(tree.insert(5).second, true);
    EXPECT_EQ(tree.insert(10).second, true);
    EXPECT_EQ(tree.insert(0).second, true);

    EXPECT_EQ(tree.size(), 3);
}

TEST(bk_tree, repeated_insert)
{
    const auto dist = [](long long a, long long b) { return (size_t)std::abs(a - b); };
    bk_tree<unsigned, decltype(dist)> tree;

    tree.insert(5);
    tree.insert(10);
    tree.insert(0);

    EXPECT_EQ(tree.insert(5).second, false);
    EXPECT_EQ(tree.insert(10).second, false);
    EXPECT_EQ(tree.insert(0).second, false);

    EXPECT_EQ(tree.size(), 3);
}



TEST(bk_tree, direct_find)
{
    const auto dist = [](long long a, long long b) { return (size_t)std::abs(a - b); };
    bk_tree<unsigned, decltype(dist)> tree;

    tree.insert(5);
    tree.insert(10);
    tree.insert(0);

    EXPECT_EQ(tree.find(5).second, 0);
    EXPECT_EQ(tree.find(10).second, 0);
    EXPECT_EQ(tree.find(0).second, 0);

    EXPECT_EQ(*tree.find(5).first, 5);
    EXPECT_EQ(*tree.find(10).first, 10);
    EXPECT_EQ(*tree.find(0).first, 0);
}

TEST(bk_tree, approx_find)
{
    const auto dist = [](long long a, long long b) { return (size_t)std::abs(a - b); };
    bk_tree<unsigned, decltype(dist)> tree;

    tree.insert(5);
    tree.insert(10);
    tree.insert(0);

    EXPECT_EQ(tree.find(1).second, 1);
    EXPECT_EQ(tree.find(2).second, 2);
    EXPECT_EQ(tree.find(3).second, 2);
    EXPECT_EQ(tree.find(4).second, 1);
    EXPECT_EQ(tree.find(5).second, 0);
    EXPECT_EQ(tree.find(6).second, 1);
    EXPECT_EQ(tree.find(7).second, 2);
    EXPECT_EQ(tree.find(8).second, 2);
    EXPECT_EQ(tree.find(9).second, 1);
    EXPECT_EQ(tree.find(10).second, 0);
    EXPECT_EQ(tree.find(15).second, 5);


    EXPECT_EQ(*tree.find(1).first, 0);
    EXPECT_EQ(*tree.find(2).first, 0);
    EXPECT_EQ(*tree.find(3).first, 5);
    EXPECT_EQ(*tree.find(4).first, 5);
    EXPECT_EQ(*tree.find(5).first, 5);
    EXPECT_EQ(*tree.find(6).first, 5);
    EXPECT_EQ(*tree.find(7).first, 5);
    EXPECT_EQ(*tree.find(8).first, 10);
    EXPECT_EQ(*tree.find(9).first, 10);
    EXPECT_EQ(*tree.find(10).first, 10);
    EXPECT_EQ(*tree.find(15).first, 10);
}

TEST(bk_tree, strings)
{
    constexpr auto sv_dist = [](std::string_view l, std::string_view r) { return levenshtein_distance(l, r); };
    bk_tree<std::string, decltype(sv_dist)> tree;

    tree.insert("lorem");
    tree.insert("ipsum");
    tree.insert("dolor");
    tree.insert("sit");
    tree.insert("amet");
    tree.insert("consectetur");
    tree.insert("adipiscing");
    tree.insert("elit");
    tree.insert("sed");
    tree.insert("do");
    tree.insert("eiusmod");
    tree.insert("tempor");
    tree.insert("incididunt");
    tree.insert("ut");
    tree.insert("labore");
    tree.insert("et");
    tree.insert("dolore");
    tree.insert("magna");
    tree.insert("aliqua");

    EXPECT_EQ(*tree.find("lorem").first, "lorem");
    EXPECT_EQ(*tree.find("loram").first, "lorem");
    EXPECT_EQ(*tree.find("dollar").first, "dolor");
    EXPECT_EQ(*tree.find("temporary").first, "tempor");
    EXPECT_EQ(*tree.find("elaborate").first, "labore");
    EXPECT_EQ(*tree.find("ett").first, "et");
    EXPECT_EQ(*tree.find("connector").first, "consectetur");
}
