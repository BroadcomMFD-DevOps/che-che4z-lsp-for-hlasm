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
#include "utils/platform.h"
#include "utils/resource_location.h"
#include "workspaces/file_manager_impl.h"
#include "workspaces/workspace.h"

using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::parser_library::workspaces;
using namespace hlasm_plugin::utils::resource;
using hlasm_plugin::utils::platform::is_windows;

namespace {
const std::string pgroups_file_pattern_absolute = is_windows() ? R"({
  "pgroups": [
    {
      "name": "P1",
      "libs": [ "C:\\Temp\\Lib", "C:\\Temp\\Lib2\\Libs\\**" ]
    }
  ]
})"
                                                               : R"({
  "pgroups": [
    {
      "name": "P1",
      "libs": [ "/home/Temp/Lib", "/home/Temp/Lib2/Libs/**" ]
    }
  ]
})";

const std::string pgroups_file_pattern_relative = is_windows() ? R"({
  "pgroups": [
    {
      "name": "P1",
      "libs": [ "pattern_test\\libs\\**" ]
    }
  ]
})"
                                                               : R"({
  "pgroups": [
    {
      "name": "P1",
      "libs": [ "pattern_test/libs/**" ]
    }
  ]
})";

const std::string pgroups_file_pattern_uri = is_windows() ? R"({
  "pgroups": [
    {
      "name": "P1",
      "libs": [ "file:///C%3A/User/ws/pattern_test/libs/**" ]
    }
  ]
})"
                                                          : R"({
  "pgroups": [
    {
      "name": "P1",
      "libs": [ "file:///home/User/ws/pattern_test/libs/**" ]
    }
  ]
})";

const std::string pgroups_file_pattern_uri_2 = is_windows() ? R"({
  "pgroups": [
    {
      "name": "P1",
      "libs": [ "file:///C%3A/User/**/pattern_*est/libs/sublib1", "file:///C%3A/User/ws/pattern_test/libs/sublib2" ]
    }
  ]
})"
                                                            : R"({
  "pgroups": [
    {
      "name": "P1",
      "libs": [ "file:///home/User/**/pattern_*est/libs/sublib1", "file:///home/User/ws/pattern_test/libs/sublib2" ]
    }
  ]
})";

const std::string pgroups_file_all_types = is_windows() ? R"({
  "pgroups": [
    {
      "name": "P1",
      "libs": [ "C:\\Temp\\Lib", "C:\\Temp\\Lib2\\Libs\\**", "different_libs", "different_libs2\\Libs\\**", "file:///C%3A/User/**/pattern_*est/libs/sublib1", "file:///C%3A/User/ws/pattern_test/libs/sublib2" ]
    }
  ]
})"
                                                        : R"({
  "pgroups": [
    {
      "name": "P1",
      "libs": [ "/home/Temp/Lib", "/home/Temp/Lib2/Libs/**", "different_libs", "different_libs2/Libs/**", "file:///home/User/**/pattern_*est/libs/sublib1", "file:///home/User/ws/pattern_test/libs/sublib2" ]
    }
  ]
})";

const std::string pgmconf_file = R"({
  "pgms": [
	{
      "program": "pattern_test/source",
      "pgroup": "P1"
    }
  ]
})";

const std::string source_txt = R"(         MACRO
         MAC
         MAC1
         MAC2
         MEND

         MAC
         MAC1
         MAC2

         END)";

enum class variants
{
    ABSOLUTE,
    RELATIVE,
    URI,
    URI_2,
    ALL_TYPES
};

const auto root_dir_loc = is_windows() ? resource_location("file:///C%3A/") : resource_location("file:///home/");
const auto user_dir_loc = resource_location::join(root_dir_loc, "User/");

const auto ws_loc = resource_location::join(user_dir_loc, "ws/");
const auto hlasmplugin_folder_loc(resource_location::join(ws_loc, ".hlasmplugin/"));
const auto proc_grps_loc(resource_location::join(hlasmplugin_folder_loc, "proc_grps.json"));
const auto pgm_conf_loc(resource_location::join(hlasmplugin_folder_loc, "pgm_conf.json"));

const auto pattern_test_dir_loc = resource_location::join(ws_loc, "pattern_test/");
const auto pattern_est_dir_loc = resource_location::join(ws_loc, "pattern_est/");
const auto patter_test_dir_loc = resource_location::join(ws_loc, "patter_test/");

const auto pattern_test_source_loc(resource_location::join(pattern_test_dir_loc, "source"));
const auto pattern_test_lib_loc(resource_location::join(pattern_test_dir_loc, "libs/"));
const auto pattern_test_lib_sublib1_loc(resource_location::join(pattern_test_lib_loc, "sublib1/"));
const auto pattern_test_lib_sublib2_loc(resource_location::join(pattern_test_lib_loc, "sublib2/"));
const auto pattern_test_macro1_loc(resource_location::join(pattern_test_lib_sublib1_loc, "mac1"));
const auto pattern_test_macro2_loc(resource_location::join(pattern_test_lib_sublib2_loc, "mac2"));

const auto temp_lib_loc = resource_location::join(root_dir_loc, "Temp/Lib/");
const auto temp_lib2_libs_loc = resource_location::join(root_dir_loc, "Temp/Lib2/Libs/");

const auto different_libs_loc = resource_location::join(ws_loc, "different_libs/");
const auto different_libs2_libs_loc = resource_location::join(ws_loc, "different_libs2/Libs/");

const auto different_libs2_libs_subdir = list_directory_result {
    { { "different_libs/subdir", resource_location::join(different_libs2_libs_loc, "subdir/") } },
    hlasm_plugin::utils::path::list_directory_rc::done
};

const auto temp_lib2_libs_subdir =
    list_directory_result { { { "temp_libs/subdir", resource_location::join(temp_lib2_libs_loc, "subdir/") } },
        hlasm_plugin::utils::path::list_directory_rc::done };

const char* pattern_lib_sublib1_abs_path =
    is_windows() ? "C:\\\\User\\ws\\pattern_test\\libs\\sublib1\\" : "/home/User/ws/pattern_test/libs/sublib1/";
const char* pattern_lib_sublib2_abs_path =
    is_windows() ? "c:\\\\User\\ws\\pAttErn_test\\libs\\sublib2\\" : "/home/User/ws/pattern_test/libs/sublib2/";

class file_manager_lib_pattern : public file_manager_impl
{
    std::string get_pgroup_file(variants variant)
    {
        switch (variant)
        {
            case variants::ABSOLUTE:
                return pgroups_file_pattern_absolute;
            case variants::RELATIVE:
                return pgroups_file_pattern_relative;
            case variants::URI:
                return pgroups_file_pattern_uri;
            case variants::URI_2:
                return pgroups_file_pattern_uri_2;
            case variants::ALL_TYPES:
                return pgroups_file_all_types;
        }
        throw std::logic_error("Not implemented");
    }

public:
    file_manager_lib_pattern(variants variant)
    {
        did_open_file(proc_grps_loc, 1, get_pgroup_file(variant));
        did_open_file(pgm_conf_loc, 1, pgmconf_file);
        did_open_file(pattern_test_source_loc, 1, source_txt);
    }

    MOCK_METHOD(list_directory_result,
        list_directory_files,
        (const hlasm_plugin::utils::resource::resource_location& path),
        (const override));

    list_directory_result list_directory_subdirs_and_symlinks(
        const hlasm_plugin::utils::resource::resource_location& location) const override
    {
        if (location == pattern_test_lib_loc)
            return { { { pattern_lib_sublib1_abs_path, pattern_test_lib_sublib1_loc },
                         { pattern_lib_sublib2_abs_path, pattern_test_lib_sublib2_loc } },
                hlasm_plugin::utils::path::list_directory_rc::done };

        if (location == user_dir_loc)
            return { { { "pattern_est", pattern_est_dir_loc },
                         { "pattern_test", pattern_test_dir_loc },
                         { "patter_test", patter_test_dir_loc } },
                hlasm_plugin::utils::path::list_directory_rc::done };

        if (location == pattern_test_dir_loc)
            return { { { "libs", pattern_test_lib_loc } }, hlasm_plugin::utils::path::list_directory_rc::done };


        if (location == different_libs2_libs_loc)
            return different_libs2_libs_subdir;

        if (location == temp_lib2_libs_loc)
            return temp_lib2_libs_subdir;

        return { {}, hlasm_plugin::utils::path::list_directory_rc::done };
    }

    bool dir_exists(const hlasm_plugin::utils::resource::resource_location&) const override { return true; }
};
} // namespace

TEST(workspace_pattern_test, absolute)
{
    file_manager_lib_pattern file_manager(variants::ABSOLUTE);
    lib_config config;

    workspace ws(ws_loc, "workspace_name", file_manager, config);
    ws.open();

    EXPECT_CALL(file_manager, list_directory_files(temp_lib_loc))
        .WillOnce(::testing::Return(list_directory_result { {}, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(temp_lib2_libs_loc))
        .WillOnce(::testing::Return(list_directory_result { {}, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(temp_lib2_libs_subdir.first.begin()->second))
        .WillOnce(::testing::Return(list_directory_result { {}, hlasm_plugin::utils::path::list_directory_rc::done }));

    ws.did_open_file(pattern_test_source_loc);
}

TEST(workspace_pattern_test, relative)
{
    file_manager_lib_pattern file_manager(variants::RELATIVE);
    lib_config config;

    workspace ws(ws_loc, "workspace_name", file_manager, config);
    ws.open();

    EXPECT_CALL(file_manager, list_directory_files(pattern_test_lib_loc))
        .WillOnce(::testing::Return(list_directory_result { {}, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(pattern_test_lib_sublib1_loc))
        .WillOnce(::testing::Return(list_directory_result {
            { { "mac1", pattern_test_macro1_loc } }, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(pattern_test_lib_sublib2_loc))
        .WillOnce(::testing::Return(list_directory_result {
            { { "mac2", pattern_test_macro2_loc } }, hlasm_plugin::utils::path::list_directory_rc::done }));

    ws.did_open_file(pattern_test_source_loc);
}

TEST(workspace_pattern_test, uri)
{
    file_manager_lib_pattern file_manager(variants::URI);
    lib_config config;

    workspace ws(ws_loc, "workspace_name", file_manager, config);
    ws.open();

    EXPECT_CALL(file_manager, list_directory_files(pattern_test_lib_loc))
        .WillOnce(::testing::Return(list_directory_result { {}, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(pattern_test_lib_sublib1_loc))
        .WillOnce(::testing::Return(list_directory_result {
            { { "mac1", pattern_test_macro1_loc } }, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(pattern_test_lib_sublib2_loc))
        .WillOnce(::testing::Return(list_directory_result {
            { { "mac2", pattern_test_macro2_loc } }, hlasm_plugin::utils::path::list_directory_rc::done }));

    ws.did_open_file(pattern_test_source_loc);
}

TEST(workspace_pattern_test, uri_2)
{
    file_manager_lib_pattern file_manager(variants::URI_2);
    lib_config config;

    workspace ws(ws_loc, "workspace_name", file_manager, config);
    ws.open();

    EXPECT_CALL(file_manager, list_directory_files(pattern_test_lib_sublib1_loc))
        .WillOnce(::testing::Return(list_directory_result {
            { { "mac1", pattern_test_macro1_loc } }, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(pattern_test_lib_sublib2_loc))
        .WillOnce(::testing::Return(list_directory_result {
            { { "mac2", pattern_test_macro2_loc } }, hlasm_plugin::utils::path::list_directory_rc::done }));

    ws.did_open_file(pattern_test_source_loc);
}

TEST(workspace_pattern_test, all_types)
{
    file_manager_lib_pattern file_manager(variants::ALL_TYPES);
    lib_config config;

    workspace ws(ws_loc, "workspace_name", file_manager, config);
    ws.open();

    EXPECT_CALL(file_manager, list_directory_files(temp_lib_loc))
        .WillOnce(::testing::Return(list_directory_result { {}, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(temp_lib2_libs_loc))
        .WillOnce(::testing::Return(list_directory_result { {}, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(temp_lib2_libs_subdir.first.begin()->second))
        .WillOnce(::testing::Return(list_directory_result { {}, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(different_libs_loc))
        .WillOnce(::testing::Return(list_directory_result { {}, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(different_libs2_libs_loc))
        .WillOnce(::testing::Return(list_directory_result { {}, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(different_libs2_libs_subdir.first.begin()->second))
        .WillOnce(::testing::Return(list_directory_result { {}, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(pattern_test_lib_sublib1_loc))
        .WillOnce(::testing::Return(list_directory_result {
            { { "mac1", pattern_test_macro1_loc } }, hlasm_plugin::utils::path::list_directory_rc::done }));
    EXPECT_CALL(file_manager, list_directory_files(pattern_test_lib_sublib2_loc))
        .WillOnce(::testing::Return(list_directory_result {
            { { "mac2", pattern_test_macro2_loc } }, hlasm_plugin::utils::path::list_directory_rc::done }));

    ws.did_open_file(pattern_test_source_loc);
}