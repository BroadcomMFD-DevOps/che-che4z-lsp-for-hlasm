/*
 * Copyright (c) 2023 Broadcom.
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

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "lexing/logical_line.h"

using namespace hlasm_plugin::parser_library::lexing;

namespace {
class logical_line_iterator_fixture : public ::testing::TestWithParam<std::vector<std::string_view>>
{};
} // namespace

TEST_P(logical_line_iterator_fixture, general_behavior)
{
    logical_line line;
    const auto& parm = GetParam();
    std::transform(parm.begin(), parm.end(), std::back_inserter(line.segments), [](const auto& c) {
        logical_line_segment lls;
        lls.code = c;
        return lls;
    });

    std::string concat_parm;
    for (auto p : parm)
        concat_parm.append(p);

    EXPECT_TRUE(std::equal(line.begin(), line.end(), concat_parm.begin(), concat_parm.end()));
    EXPECT_TRUE(std::equal(std::make_reverse_iterator(line.end()),
        std::make_reverse_iterator(line.begin()),
        concat_parm.rbegin(),
        concat_parm.rend()));
}

INSTANTIATE_TEST_SUITE_P(logical_line,
    logical_line_iterator_fixture,
    ::testing::ValuesIn(std::vector {
        std::vector<std::string_view> {},
        std::vector<std::string_view> { "" },
        std::vector<std::string_view> { "", "" },
        std::vector<std::string_view> { "a", "b" },
        std::vector<std::string_view> { "", "b" },
        std::vector<std::string_view> { "a", "" },
        std::vector<std::string_view> { "a", "", "c" },
        std::vector<std::string_view> { "a", "", "c", "" },
        std::vector<std::string_view> { "a", "", "c", "", "e" },
        std::vector<std::string_view> { "", "", "abc", "", "", "def", "", "", "ghi", "", "" },
    }));

namespace {
class logical_line_iterator_transform_test : public testing::Test
{
public:
    logical_line_iterator_transform_test(std::string_view input)
        : m_input(std::move(input))
    {}

    void SetUp() override
    {
        ASSERT_TRUE(extract_logical_line(m_line_a, m_input, default_ictl));
        m_line_b = m_line_a;
    }

protected:
    logical_line m_line_a;
    logical_line m_line_b;
    std::string_view m_input;

    void check_equality(const logical_line::const_iterator& it_regular,
        const logical_line::const_iterator& it_transformed,
        const logical_line& base_line)
    {
        EXPECT_TRUE(std::equal(it_regular, base_line.end(), it_transformed, base_line.end()));
        EXPECT_TRUE(std::equal(std::make_reverse_iterator(it_regular),
            std::make_reverse_iterator(base_line.begin()),
            std::make_reverse_iterator(it_transformed),
            std::make_reverse_iterator(base_line.begin())));
    };
};

class logical_line_iterator_transform_single_line_test : public logical_line_iterator_transform_test
{
public:
    logical_line_iterator_transform_single_line_test()
        : logical_line_iterator_transform_test("123456")
    {}
};
} // namespace

TEST_F(logical_line_iterator_transform_single_line_test, unchanged_code_part)
{
    auto it_transformed_a = m_line_b.begin().transform_into(m_line_a);
    auto it_regular_a = m_line_a.begin();
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = std::next(m_line_b.begin(), 3).transform_into(m_line_a);
    it_regular_a = std::next(m_line_a.begin(), 3);
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = m_line_b.end().transform_into(m_line_a);
    it_regular_a = m_line_a.end();
    check_equality(it_regular_a, it_transformed_a, m_line_a);
}

TEST_F(logical_line_iterator_transform_single_line_test, removed_code_prefix)
{
    m_line_b.segments.front().code.remove_prefix(3);

    auto it_transformed_a = m_line_b.begin().transform_into(m_line_a);
    auto it_regular_a = std::next(m_line_a.begin(), 3);
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = m_line_b.end().transform_into(m_line_a);
    it_regular_a = m_line_a.end();
    check_equality(it_regular_a, it_transformed_a, m_line_a);
}

TEST_F(logical_line_iterator_transform_single_line_test, removed_code_suffix)
{
    m_line_b.segments.front().code.remove_suffix(3);

    auto it_transformed_a = m_line_b.begin().transform_into(m_line_a);
    auto it_regular_a = m_line_a.begin();
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = m_line_b.end().transform_into(m_line_a);
    it_regular_a = std::next(m_line_a.begin(), 3);
    check_equality(it_regular_a, it_transformed_a, m_line_a);
}

TEST_F(logical_line_iterator_transform_single_line_test, removed_code_prefix_suffix)
{
    m_line_b.segments.front().code.remove_prefix(1);
    m_line_b.segments.front().code.remove_suffix(1);

    auto it_transformed_a = m_line_b.begin().transform_into(m_line_a);
    auto it_regular_a = std::next(m_line_a.begin());
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = m_line_b.end().transform_into(m_line_a);
    it_regular_a = std::prev(m_line_a.end());
    check_equality(it_regular_a, it_transformed_a, m_line_a);
}

namespace {
class logical_line_iterator_transform_multi_line_test : public logical_line_iterator_transform_test
{
public:
    logical_line_iterator_transform_multi_line_test()
        : logical_line_iterator_transform_test(m_input)
    {}

private:
    inline static const std::string_view m_input =
        R"(                  EXEC      SQL                                        X00004000
               --comment                                               X
               SELECT                                                  X
               1       --rem                                           X00050000
                   INTO :B                                             X
               FROM                                                    X
               SYSIBM.SYSDUMMY1)";
};
} // namespace

TEST_F(logical_line_iterator_transform_multi_line_test, unchanged_code_part)
{
    auto it_transformed_a = m_line_b.begin().transform_into(m_line_a);
    auto it_regular_a = m_line_a.begin();
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = std::next(m_line_b.begin(), 120).transform_into(m_line_a);
    it_regular_a = std::next(m_line_a.begin(), 120);
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = m_line_b.end().transform_into(m_line_a);
    it_regular_a = m_line_a.end();
    check_equality(it_regular_a, it_transformed_a, m_line_a);
}

TEST_F(logical_line_iterator_transform_multi_line_test, empty_all_lines)
{
    std::for_each(m_line_b.segments.begin(), m_line_b.segments.end(), [](auto& s) { s.code = {}; });

    auto it_transformed_a = m_line_b.begin().transform_into(m_line_a);
    auto it_regular_a = m_line_a.end();
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = m_line_b.end().transform_into(m_line_a);
    it_regular_a = m_line_a.end();
    check_equality(it_regular_a, it_transformed_a, m_line_a);
}

TEST_F(logical_line_iterator_transform_multi_line_test, empty_last_line)
{
    m_line_b.segments.back().code = {};

    auto it_transformed_a = m_line_b.begin().transform_into(m_line_a);
    auto it_regular_a = m_line_a.begin();
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = m_line_b.end().transform_into(m_line_a);
    it_regular_a = std::prev(m_line_a.end(), 16);
    check_equality(it_regular_a, it_transformed_a, m_line_a);
}

TEST_F(logical_line_iterator_transform_multi_line_test, empty_some_lines)
{
    m_line_b.segments[1].code = {};
    m_line_b.segments[3].code.remove_suffix(46);

    auto it_transformed_a = m_line_b.begin().transform_into(m_line_a);
    auto it_regular_a = m_line_a.begin();
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = std::next(m_line_b.begin(), 70).transform_into(m_line_a);
    it_regular_a = std::next(m_line_a.begin(), 70);
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = std::next(m_line_b.begin(), 71).transform_into(m_line_a);
    it_regular_a = std::next(m_line_a.begin(), 127);
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = std::next(m_line_b.begin(), 136).transform_into(m_line_a);
    it_regular_a = std::next(m_line_a.begin(), 192);
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = std::next(m_line_b.begin(), 137).transform_into(m_line_a);
    it_regular_a = std::next(m_line_a.begin(), 239);
    check_equality(it_regular_a, it_transformed_a, m_line_a);

    it_transformed_a = m_line_b.end().transform_into(m_line_a);
    it_regular_a = m_line_a.end();
    check_equality(it_regular_a, it_transformed_a, m_line_a);
}
