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

#include "utils/platform.h"
#include "workspaces/wildcard.h"

using namespace hlasm_plugin::parser_library::workspaces;

TEST(wildcard2regex_test, general)
{
    std::string test = "this is a test sentence.";

    auto regex = wildcard2regex("*test*");
    EXPECT_TRUE(std::regex_match(test, regex));

    regex = wildcard2regex("*.");
    EXPECT_TRUE(std::regex_match(test, regex));

    regex = wildcard2regex("this is a test ?entence.");
    EXPECT_TRUE(std::regex_match(test, regex));

    regex = wildcard2regex("*.?");
    EXPECT_FALSE(std::regex_match(test, regex));
}

TEST(wildcard2regex_test, path)
{
    auto regex = wildcard2regex("pgms/*");
    EXPECT_TRUE(std::regex_match("pgms/anything", regex));

    regex = wildcard2regex("pgms\\*");
    EXPECT_TRUE(std::regex_match("pgms/anything", regex));
}

namespace {
void verify_file_scheme(std::string colon)
{
    auto regex = wildcard2regex("file:///C" + colon + "/dir/*");

    EXPECT_TRUE(std::regex_match("file:///C:/dir/whatever/file", regex));
    EXPECT_TRUE(std::regex_match("file:///C:/dir/", regex));
    EXPECT_TRUE(std::regex_match("file:///c:/dir/whatever/file", regex));
    EXPECT_TRUE(std::regex_match("file:///c:/dir/", regex));
    EXPECT_TRUE(std::regex_match("file:///C%3A/dir/whatever/file", regex));
    EXPECT_TRUE(std::regex_match("file:///C%3A/dir/", regex));
    EXPECT_TRUE(std::regex_match("file:///C%3a/dir/whatever/file", regex));
    EXPECT_TRUE(std::regex_match("file:///C%3a/dir/", regex));

    EXPECT_FALSE(std::regex_match("file:///D:/dir/", regex));
    EXPECT_FALSE(std::regex_match("file:///D%3A/dir/", regex));
    EXPECT_FALSE(std::regex_match("file:///D%3a/dir/", regex));
}
} // namespace

TEST(wildcard2regex_test, file_scheme)
{
    if (hlasm_plugin::utils::platform::is_windows())
    {
        verify_file_scheme(":");
        verify_file_scheme("%3A");
        verify_file_scheme("%3a");
    }
}

TEST(wildcard2regex_test, utf_8_chars_01)
{
    auto regex = wildcard2regex("pg?s");
    EXPECT_TRUE(std::regex_match("pgms", regex));
    EXPECT_TRUE(std::regex_match("pg%7fs", regex));
    EXPECT_TRUE(std::regex_match("pg%cf%bfs", regex));
    EXPECT_TRUE(std::regex_match("pg%ef%bf%bfs", regex));
    EXPECT_TRUE(std::regex_match("pg%f0%9f%a7%bfs", regex));

    EXPECT_FALSE(std::regex_match("pg%24%25s", regex));
    EXPECT_FALSE(std::regex_match("pg%C3%BF%25s", regex));
    EXPECT_FALSE(std::regex_match("pg%C3%BF%C3%BEs", regex));
    EXPECT_FALSE(std::regex_match("pg%DF%BF%25s", regex));

    // %FF is not a valid UTF-8 character
    EXPECT_FALSE(std::regex_match("pg%Ffs", regex));
}

TEST(wildcard2regex_test, utf_8_chars_02)
{
    auto regex = wildcard2regex("pg??s");
    EXPECT_FALSE(std::regex_match("pgms", regex));
    EXPECT_FALSE(std::regex_match("pg%7fs", regex));
    EXPECT_FALSE(std::regex_match("pg%cf%bfs", regex));
    EXPECT_FALSE(std::regex_match("pg%ef%bf%bfs", regex));
    EXPECT_FALSE(std::regex_match("pg%f0%9f%a7%bfs", regex));

    EXPECT_TRUE(std::regex_match("pg%24%25s", regex));
    EXPECT_TRUE(std::regex_match("pg%C3%BF%25s", regex));
    EXPECT_TRUE(std::regex_match("pg%C3%BF%C3%BEs", regex));
    EXPECT_TRUE(std::regex_match("pg%DF%BF%25s", regex));

    // %FF is not a valid UTF-8 character
    EXPECT_FALSE(std::regex_match("pg%Ff%Ffs", regex));
}