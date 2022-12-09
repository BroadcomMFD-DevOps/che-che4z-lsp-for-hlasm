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

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "../common_testing.h"
#include "analyzer.h"
#include "debugging/debug_lib_provider.h"
#include "utils/resource_location.h"
#include "workspaces/library.h"

using namespace ::testing;
using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::parser_library::debugging;
using namespace hlasm_plugin::parser_library::workspaces;
using namespace hlasm_plugin::utils::resource;

namespace {
class library_mock : public library
{
public:
    // Inherited via library
    MOCK_METHOD(std::shared_ptr<processor>, find_file, (std::string_view), (override));
    MOCK_METHOD(void, refresh, (), (override));
    MOCK_METHOD(std::vector<std::string>, list_files, (), (override));
    MOCK_METHOD(std::string, refresh_url_prefix, (), (const override));
    MOCK_METHOD((std::pair<resource_location, std::string>), get_file_content, (std::string_view), (override));
    MOCK_METHOD(bool, has_file, (std::string_view), (override));
    MOCK_METHOD(void, copy_diagnostics, (std::vector<diagnostic_s>&), (const override));
};

class debug_lib_provider_test : public Test
{
protected:
    std::shared_ptr<NiceMock<library_mock>> mock_lib = std::make_shared<NiceMock<library_mock>>();
    debug_lib_provider lib = debug_lib_provider({ mock_lib }, nullptr);
};

} // namespace

TEST_F(debug_lib_provider_test, parse_library)
{
    const std::string aaa_content = " MNOTE 'AAA'";
    const resource_location aaa_locaiton("AAA");
    EXPECT_CALL(*mock_lib, get_file_content(Eq("AAA"))).WillOnce(Return(std::pair(aaa_locaiton, aaa_content)));

    std::string input = " COPY AAA";
    analyzer a(input, analyzer_options(&lib));
    a.analyze();
    a.collect_diags();

    EXPECT_TRUE(matches_message_codes(a.diags(), { "MNOTE" }));
}

TEST_F(debug_lib_provider_test, has_library)
{
    EXPECT_CALL(*mock_lib, has_file(Eq("AAA"))).WillOnce(Return(true));
    EXPECT_CALL(*mock_lib, has_file(Eq("BBB"))).WillOnce(Return(false));

    EXPECT_TRUE(lib.has_library("AAA", resource_location()));
    EXPECT_FALSE(lib.has_library("BBB", resource_location()));
}

TEST_F(debug_lib_provider_test, get_library)
{
    const std::string aaa_content = "AAA content";
    const resource_location aaa_locaiton("AAA");
    EXPECT_CALL(*mock_lib, get_file_content(Eq("AAA"))).WillOnce(Return(std::pair(aaa_locaiton, aaa_content)));
    EXPECT_CALL(*mock_lib, get_file_content(Eq("BBB"))).WillOnce(Return(std::pair<resource_location, std::string>()));

    EXPECT_EQ(lib.get_library("AAA", resource_location()), std::pair(aaa_content, aaa_locaiton));

    EXPECT_EQ(lib.get_library("BBB", resource_location()), std::nullopt);
}
