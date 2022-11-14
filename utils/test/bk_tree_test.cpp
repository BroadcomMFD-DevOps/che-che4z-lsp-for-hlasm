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

template<typename T>
struct abs_value
{
    auto operator()(T a, T b) const noexcept { return (size_t)std::abs(a - b); }
};

struct lv_dist_sv
{
    auto operator()(std::string_view l, std::string_view r) const noexcept { return levenshtein_distance(l, r); }
};

TEST(bk_tree, simple_insert)
{
    bk_tree<unsigned, abs_value<long long>> tree;

    EXPECT_EQ(tree.insert(5).second, true);
    EXPECT_EQ(tree.insert(10).second, true);
    EXPECT_EQ(tree.insert(0).second, true);

    EXPECT_EQ(tree.size(), 3);
}

TEST(bk_tree, repeated_insert)
{
    bk_tree<unsigned, abs_value<long long>> tree;

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
    bk_tree<unsigned, abs_value<long long>> tree;

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
    bk_tree<unsigned, abs_value<long long>> tree;

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
    bk_tree<std::string, lv_dist_sv> tree;

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

TEST(bk_tree, max_dist)
{
    bk_tree<std::string, lv_dist_sv> tree;

    tree.insert("lorem");
    tree.insert("ipsum");
    tree.insert("dolor");

    EXPECT_EQ(tree.find("", 1).first, nullptr);
    EXPECT_NE(tree.find("loram", 1).first, nullptr);
}

TEST(bk_tree, multiple_results)
{
    bk_tree<std::string, lv_dist_sv> tree;

    tree.insert("abc1");
    tree.insert("abc2");
    tree.insert("abc3");
    tree.insert("abcdd");

    auto r = tree.find<3>("abc", 1);

    ASSERT_TRUE(std::all_of(r.begin(), r.end(), [](const auto& v) { return v.first != nullptr; }));
    std::vector<std::string> result;
    std::transform(r.begin(), r.end(), std::back_inserter(result), [](const auto& v) { return *v.first; });
    const std::string expected[] { "abc1", "abc2", "abc3" };

    EXPECT_TRUE(std::is_permutation(result.begin(), result.end(), std::begin(expected), std::end(expected)));
}

TEST(bk_tree, multiple_results_no_limit)
{
    bk_tree<std::string, lv_dist_sv> tree;

    tree.insert("abc1");
    tree.insert("abc2");
    tree.insert("abc3");
    tree.insert("abcdd");

    auto r = tree.find<3>("abc");

    ASSERT_TRUE(std::all_of(r.begin(), r.end(), [](const auto& v) { return v.first != nullptr; }));
    std::vector<std::string> result;
    std::transform(r.begin(), r.end(), std::back_inserter(result), [](const auto& v) { return *v.first; });
    const std::string expected[] { "abc1", "abc2", "abc3" };

    EXPECT_TRUE(std::is_permutation(result.begin(), result.end(), std::begin(expected), std::end(expected)));
}
