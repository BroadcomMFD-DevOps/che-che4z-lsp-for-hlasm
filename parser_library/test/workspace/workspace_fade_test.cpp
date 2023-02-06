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
#include <initializer_list>
#include <regex>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "../common_testing.h"
#include "consume_diagnostics_mock.h"
#include "empty_configs.h"
#include "fade_messages.h"
#include "lib_config.h"
#include "utils/resource_location.h"
#include "workspaces/file_manager_impl.h"
#include "workspaces/workspace.h"

using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::parser_library::workspaces;
using namespace hlasm_plugin::utils::resource;

namespace {

const resource_location src1_loc("src1.hlasm");
const resource_location src2_loc("src2.hlasm");
const resource_location pgm_conf_loc(".hlasmplugin/pgm_conf.json");
const resource_location proc_grps_loc(".hlasmplugin/proc_grps.json");
const resource_location cpybook_loc("libs/CPYBOOK");
const resource_location mac_loc("libs/mac");

class diags_retriever : public diagnosable_impl
{
public:
    void collect_diags() const override {}
    size_t collect_and_get_diags_size(workspace& ws, file_manager& file_mngr)
    {
        diags().clear();
        collect_diags_from_child(ws);
        collect_diags_from_child(file_mngr);
        return diags().size();
    }
};

class file_manager_extended2 : public file_manager_impl
{
public:
    list_directory_result list_directory_files(const hlasm_plugin::utils::resource::resource_location&) const override
    {
        return { { { "CPYBOOK", cpybook_loc }, { "MAC", mac_loc } },
            hlasm_plugin::utils::path::list_directory_rc::done };
    }
};

} // namespace

namespace {
struct test_params
{
    std::vector<std::string> text_to_insert;
    std::vector<fade_message_s> expected_fade_messages;
    size_t number_of_diags = 0;
};

class fade_fixture_opencode_base : public diags_retriever, public ::testing::TestWithParam<test_params>
{
public:
    file_manager_extended2 file_manager;
    lib_config config;
    shared_json global_settings = make_empty_shared_json();
    workspace ws;
    std::vector<fade_message_s> fms;

    fade_fixture_opencode_base()
        : ws(workspace(file_manager, config, global_settings))
    {}

    void SetUp() override
    {
        file_manager.did_open_file(pgm_conf_loc, 1, pgm_conf);
        file_manager.did_open_file(proc_grps_loc, 1, proc_grps);
    }

    void collect_fms()
    {
        fms.clear();
        file_manager.retrieve_fade_messages(fms);
    }

    void open_file(resource_location rl, std::string text)
    {
        file_manager.did_open_file(rl, 1, std::move(text));
        ws.did_open_file(rl);
    }

    void open_src_files_and_collect_fms(std::initializer_list<std::pair<resource_location, std::string>> files)
    {
        ws.open();

        for (const auto& [rl, text] : files)
        {
            file_manager.did_open_file(rl, 1, text);
            ws.did_open_file(rl);
        }

        collect_fms();
    }

private:
    std::string pgm_conf = R"({
  "pgms": [
    {
      "program": "src?.hlasm",
      "pgroup": "P1"
    }
  ]
})";

    std::string proc_grps = R"({
  "pgroups": [
    {
      "name": "P1",
      "libs": ["libs"]
    }
  ]
})";
};

class fade_fixture_opencode_general : public fade_fixture_opencode_base
{};

INSTANTIATE_TEST_SUITE_P(fade,
    fade_fixture_opencode_general,
    ::testing::Values(test_params { { "0" }, {} },
        test_params { { "1" },
            { fade_message_s::inactive_statement("src1.hlasm", range(position(2, 0), position(2, 72))),
                fade_message_s::inactive_statement("src1.hlasm", range(position(3, 0), position(4, 72))) } }));

} // namespace

TEST_P(fade_fixture_opencode_general, opencode)
{
    std::string src1 = R"(
         AIF ($x EQ 1).SKIP
&A       SETA 5
&C       SETC '12345678901234567890123456789012345678901234567890123456X
               789012345678901234567890'
.SKIP    ANOP

         END)";

    src1 = std::regex_replace(src1, std::regex("\\$x"), GetParam().text_to_insert[0]);

    open_src_files_and_collect_fms({ { src1_loc, std::move(src1) } });

    EXPECT_EQ(collect_and_get_diags_size(ws, file_manager), GetParam().number_of_diags);
    EXPECT_EQ(ws.diags().size(), (size_t)0);

    auto& expected_msgs = GetParam().expected_fade_messages;
    EXPECT_TRUE(std::is_permutation(fms.begin(),
        fms.end(),
        expected_msgs.begin(),
        expected_msgs.end(),
        [](const auto& fmsg, const auto& expected_fmsg) {
            return fmsg.code == expected_fmsg.code && fmsg.r == expected_fmsg.r && fmsg.uri == expected_fmsg.uri;
        }));
}

namespace {
class fade_fixture_opencode_macros_opencode : public fade_fixture_opencode_base
{};

INSTANTIATE_TEST_SUITE_P(fade,
    fade_fixture_opencode_macros_opencode,
    ::testing::Values(test_params { { "         MAC 0" }, {} },
        test_params { { "         MAC 1" },
            { fade_message_s::inactive_statement("src1.hlasm", range(position(4, 0), position(4, 72))) } },
        test_params { { "*        MAC 1" },
            { fade_message_s::inactive_statement("src1.hlasm", range(position(3, 0), position(3, 72))),
                fade_message_s::inactive_statement("src1.hlasm", range(position(4, 0), position(4, 72))),
                fade_message_s::inactive_statement("src1.hlasm", range(position(5, 0), position(5, 72))),
                fade_message_s::inactive_statement("src1.hlasm", range(position(6, 0), position(6, 72))) } }));
} // namespace

TEST_P(fade_fixture_opencode_macros_opencode, macros_opencode)
{
    std::string src1 = R"(
         MACRO
         MAC  &P
         AIF (&P EQ 1).SKIP
         ANOP
.SKIP    ANOP
         MEND

$x

         END)";

    src1 = std::regex_replace(src1, std::regex("\\$x"), GetParam().text_to_insert[0]);

    open_src_files_and_collect_fms({ { src1_loc, std::move(src1) } });

    EXPECT_EQ(collect_and_get_diags_size(ws, file_manager), GetParam().number_of_diags);
    EXPECT_EQ(ws.diags().size(), (size_t)0);

    auto& expected_msgs = GetParam().expected_fade_messages;
    EXPECT_TRUE(std::is_permutation(fms.begin(),
        fms.end(),
        expected_msgs.begin(),
        expected_msgs.end(),
        [](const auto& fmsg, const auto& expected_fmsg) {
            return fmsg.code == expected_fmsg.code && fmsg.r == expected_fmsg.r && fmsg.uri == expected_fmsg.uri;
        }));
}

namespace {
class fade_fixture_opencode_macros_external : public fade_fixture_opencode_base
{};

INSTANTIATE_TEST_SUITE_P(fade,
    fade_fixture_opencode_macros_external,
    ::testing::Values(test_params { { "         MAC 0" }, {} },
        test_params { { "         MAC 1" },
            { fade_message_s::inactive_statement("libs/mac", range(position(3, 0), position(3, 72))) } },
        test_params { { "*        MAC 1" }, {} }));
} // namespace

TEST_P(fade_fixture_opencode_macros_external, macros_external)
{
    std::string mac = R"(         MACRO
         MAC  &P
         AIF (&P EQ 1).SKIP
         ANOP
.SKIP    ANOP
         MEND)";

    std::string src1 = R"(
$x

         END)";

    src1 = std::regex_replace(src1, std::regex("\\$x"), GetParam().text_to_insert[0]);

    open_src_files_and_collect_fms({ { mac_loc, std::move(mac) }, { src1_loc, std::move(src1) } });

    EXPECT_EQ(collect_and_get_diags_size(ws, file_manager), GetParam().number_of_diags);
    EXPECT_EQ(ws.diags().size(), (size_t)0);

    auto& expected_msgs = GetParam().expected_fade_messages;
    EXPECT_TRUE(std::is_permutation(fms.begin(),
        fms.end(),
        expected_msgs.begin(),
        expected_msgs.end(),
        [](const auto& fmsg, const auto& expected_fmsg) {
            return fmsg.code == expected_fmsg.code && fmsg.r == expected_fmsg.r && fmsg.uri == expected_fmsg.uri;
        }));
}

namespace {
class fade_fixture_cpybooks : public fade_fixture_opencode_base
{};

INSTANTIATE_TEST_SUITE_P(fade,
    fade_fixture_cpybooks,
    ::testing::Values(test_params { { "0", "0" }, {} },
        test_params { { "0", "1" }, {} },
        test_params { { "1", "0" }, {} },
        test_params { { "1", "1" },
            { fade_message_s::inactive_statement("libs/CPYBOOK", range(position(2, 0), position(2, 72))) } }));
} // namespace

TEST_P(fade_fixture_cpybooks, cpybook)
{
    std::string cpybook = R"(
         AIF (&VAR EQ 1).SKIP
LABEL    L 1,1
.SKIP    ANOP)";

    std::string src_base = R"(
&VAR     SETA  $x
         COPY CPYBOOK
         END)";

    auto src1 = std::regex_replace(src_base, std::regex("\\$x"), GetParam().text_to_insert[0]);
    auto src2 = std::regex_replace(src_base, std::regex("\\$x"), GetParam().text_to_insert[1]);

    open_src_files_and_collect_fms(
        { { cpybook_loc, std::move(cpybook) }, { src1_loc, std::move(src1) }, { src2_loc, std::move(src2) } });

    EXPECT_EQ(collect_and_get_diags_size(ws, file_manager), GetParam().number_of_diags);
    EXPECT_EQ(ws.diags().size(), (size_t)0);

    auto& expected_msgs = GetParam().expected_fade_messages;
    EXPECT_TRUE(std::is_permutation(fms.begin(),
        fms.end(),
        expected_msgs.begin(),
        expected_msgs.end(),
        [](const auto& fmsg, const auto& expected_fmsg) {
            return fmsg.code == expected_fmsg.code && fmsg.r == expected_fmsg.r && fmsg.uri == expected_fmsg.uri;
        }));
}

namespace {
class fade_fixture_opencode_nested : public fade_fixture_opencode_base
{};

INSTANTIATE_TEST_SUITE_P(fade,
    fade_fixture_opencode_nested,
    ::testing::Values(test_params { { "         MAC 0,0" }, {} },
        test_params { { "         MAC 0,1" },
            { fade_message_s::inactive_statement("libs/CPYBOOK", range(position(2, 0), position(2, 72))),
                fade_message_s::inactive_statement("libs/mac", range(position(4, 0), position(4, 72))) } },
        test_params { { "         MAC 1,0" },
            { fade_message_s::inactive_statement("libs/CPYBOOK", range(position(0, 0), position(0, 72))),
                fade_message_s::inactive_statement("libs/CPYBOOK", range(position(1, 0), position(1, 72))),
                fade_message_s::inactive_statement("libs/CPYBOOK", range(position(2, 0), position(2, 72))),
                fade_message_s::inactive_statement("libs/mac", range(position(3, 0), position(3, 72))) } },
        test_params { { "         MAC 1,1" },
            { fade_message_s::inactive_statement("libs/CPYBOOK", range(position(0, 0), position(0, 72))),
                fade_message_s::inactive_statement("libs/CPYBOOK", range(position(1, 0), position(1, 72))),
                fade_message_s::inactive_statement("libs/CPYBOOK", range(position(2, 0), position(2, 72))),
                fade_message_s::inactive_statement("libs/mac", range(position(3, 0), position(3, 72))) } },
        test_params { { "*        MAC 1,1" }, {}, 2 })); // Diags related to missing members in mac and cpybook
} // namespace

TEST_P(fade_fixture_opencode_nested, nested)
{
    std::string cpybook = R"(
         AIF (&P2 EQ 1).SKIP2
LABEL    L 1,1)";

    std::string mac = R"(         MACRO
         MAC  &P1,&P2
         AIF (&P1 EQ 1).SKIP
         COPY CPYBOOK
.SKIP    ANOP
.SKIP2   ANOP
         MEND)";

    std::string src1 = R"(
$x

         END)";

    src1 = std::regex_replace(src1, std::regex("\\$x"), GetParam().text_to_insert[0]);

    open_src_files_and_collect_fms(
        { { mac_loc, std::move(mac) }, { cpybook_loc, std::move(cpybook) }, { src1_loc, std::move(src1) } });

    EXPECT_EQ(collect_and_get_diags_size(ws, file_manager), GetParam().number_of_diags);
    EXPECT_EQ(ws.diags().size(), (size_t)0);

    auto& expected_msgs = GetParam().expected_fade_messages;
    EXPECT_TRUE(std::is_permutation(fms.begin(),
        fms.end(),
        expected_msgs.begin(),
        expected_msgs.end(),
        [](const auto& fmsg, const auto& expected_fmsg) {
            return fmsg.code == expected_fmsg.code && fmsg.r == expected_fmsg.r && fmsg.uri == expected_fmsg.uri;
        }));
}

TEST(fade, preprocessor)
{
    workspace_manager ws_mngr;
    diag_consumer_mock consumer;
    ws_mngr.register_diagnostics_consumer(&consumer);

    ws_mngr.add_workspace("workspace", "test/library/test_wks");
    std::string pgm_conf = R"({
  "pgms": [
    {
      "program": "file*",
      "pgroup": "P1"
    }
  ]
})";

    std::string proc_grps = R"({
  "pgroups": [
    {
      "name": "P1",
      "preprocessor":[{
          "name": "CICS",
          "options": [
            "NOEPILOG",
            "NOPROLOG"
          ]
        }],
      "libs": []
    }
  ]
})";
    std::string f1 = R"(
         USING *,12
         MACRO
         DFHECALL
         MEND

         L     0,DFHVALUE ( BUSY )

         DFHECALL
         END)";

    ws_mngr.did_open_file("test/library/test_wks/.hlasmplugin/pgm_conf.json", 1, pgm_conf.c_str(), pgm_conf.size());
    ws_mngr.did_open_file("test/library/test_wks/.hlasmplugin/proc_grps.json", 1, proc_grps.c_str(), proc_grps.size());
    ws_mngr.did_open_file("test/library/test_wks/file_1", 1, f1.c_str(), f1.size());
    EXPECT_EQ(consumer.diags.diagnostics_size(), static_cast<size_t>(0));
    ASSERT_EQ(consumer.fms.size(), static_cast<size_t>(1));
    EXPECT_EQ(std::string(consumer.fms.message(0).file_uri()), "test/library/test_wks/file_1");
    EXPECT_EQ(consumer.fms.message(0).get_range(), range(position(6, 0), position(6, 34)));

    std::vector<document_change> changes;
    ws_mngr.did_change_file("test/library/test_wks/file_1", 2, changes.data(), 0);
    EXPECT_EQ(consumer.diags.diagnostics_size(), static_cast<size_t>(0));
    ASSERT_EQ(consumer.fms.size(), static_cast<size_t>(1));
    EXPECT_EQ(std::string(consumer.fms.message(0).file_uri()), "test/library/test_wks/file_1");
    EXPECT_EQ(consumer.fms.message(0).get_range(), range(position(6, 0), position(6, 34)));

    std::string new_f1_text = "         L     0,DFHVALUE(BUSY) ";
    changes.push_back(document_change({ { 6, 0 }, { 6, 34 } }, new_f1_text.c_str(), new_f1_text.size()));
    ws_mngr.did_change_file("test/library/test_wks/file_1", 3, changes.data(), 1);
    EXPECT_EQ(consumer.diags.diagnostics_size(), static_cast<size_t>(0));
    ASSERT_EQ(consumer.fms.size(), static_cast<size_t>(1));
    EXPECT_EQ(std::string(consumer.fms.message(0).file_uri()), "test/library/test_wks/file_1");
    EXPECT_EQ(consumer.fms.message(0).get_range(), range(position(6, 0), position(6, 31)));

    std::string f2 = "";
    ws_mngr.did_open_file("test/library/test_wks/diff_file_2", 1, f2.c_str(), f2.size());
    EXPECT_EQ(consumer.diags.diagnostics_size(), static_cast<size_t>(0));
    ASSERT_EQ(consumer.fms.size(), static_cast<size_t>(1));
    EXPECT_EQ(std::string(consumer.fms.message(0).file_uri()), "test/library/test_wks/file_1");
    EXPECT_EQ(consumer.fms.message(0).get_range(), range(position(6, 0), position(6, 31)));

    new_f1_text = "         L     0,DFH(BUSY)";
    changes.clear();
    changes.push_back(document_change({ { 6, 0 }, { 6, 31 } }, new_f1_text.c_str(), new_f1_text.size()));
    ws_mngr.did_change_file("test/library/test_wks/file_1", 4, changes.data(), 1);
    EXPECT_GE(consumer.diags.diagnostics_size(), static_cast<size_t>(0));
    EXPECT_EQ(consumer.fms.size(), static_cast<size_t>(0));

    new_f1_text = R"(         
         MACRO
         DFHECALL
         MEND

         DFHECALL
         END)";
    changes.clear();
    changes.push_back(document_change(new_f1_text.c_str(), new_f1_text.size()));
    ws_mngr.did_change_file("test/library/test_wks/file_1", 5, changes.data(), 1);
    EXPECT_EQ(consumer.diags.diagnostics_size(), static_cast<size_t>(0));
    EXPECT_EQ(consumer.fms.size(), static_cast<size_t>(0));
}
