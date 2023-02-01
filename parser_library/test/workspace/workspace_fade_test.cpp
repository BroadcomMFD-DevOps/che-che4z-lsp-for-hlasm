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

#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "../common_testing.h"
#include "consume_diagnostics_mock.h"
#include "empty_configs.h"
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

class file_manager_extended : public file_manager_impl
{
    std::string pgmconf_file = R"({
  "pgms": [
    {
      "program": "src?.hlasm",
      "pgroup": "PG"
    }
  ]
})";

    std::string pgroups_file = R"({
  "pgroups": [
    {
    "name": "PG",
    "libs": ["libs"]
    }
  ]
})";

    std::string cpybook = R"(
         AIF (1 EQ &VAR).SKIP
LABEL    L 1,1
.SKIP    ANOP)";

    std::string val_0 = R"(
&VAR     SETA  0
         COPY CPYBOOK
         END)";

    std::string val_1 = R"(
&VAR     SETA  1
         COPY CPYBOOK
         END)";

public:
    enum class text_variant
    {
        VALUE_0,
        VALUE_1,
    };

    file_manager_extended(text_variant file1_var, text_variant file2_var)
    {
        did_open_file(proc_grps_loc, 1, pgroups_file);
        did_open_file(pgm_conf_loc, 1, pgmconf_file);
        did_open_file(cpybook_loc, 1, cpybook);

        did_open_file(src1_loc, 1, file1_var == text_variant::VALUE_0 ? val_0 : val_1);
        did_open_file(src2_loc, 1, file2_var == text_variant::VALUE_0 ? val_0 : val_1);
    }

    list_directory_result list_directory_files(const hlasm_plugin::utils::resource::resource_location&) const override
    {
        return { { { "CPYBOOK", cpybook_loc } }, hlasm_plugin::utils::path::list_directory_rc::done };
    }
};

struct test_param
{
    file_manager_extended::text_variant file1_var;
    file_manager_extended::text_variant file2_var;
    size_t expected_fms;
};

class fade_fixture : public diagnosable_impl, public ::testing::TestWithParam<test_param>
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

INSTANTIATE_TEST_SUITE_P(fade,
    fade_fixture,
    ::testing::Values(
        test_param { file_manager_extended::text_variant::VALUE_0, file_manager_extended::text_variant::VALUE_0, 0 },
        test_param { file_manager_extended::text_variant::VALUE_1, file_manager_extended::text_variant::VALUE_0, 0 },
        test_param { file_manager_extended::text_variant::VALUE_0, file_manager_extended::text_variant::VALUE_1, 0 },
        test_param { file_manager_extended::text_variant::VALUE_1, file_manager_extended::text_variant::VALUE_1, 1 }));

} // namespace

TEST_P(fade_fixture, general)
{
    file_manager_extended file_manager(GetParam().file1_var, GetParam().file2_var);

    lib_config config;
    shared_json global_settings = make_empty_shared_json();
    workspace ws(file_manager, config, global_settings);
    ws.open();
    ws.did_open_file(src1_loc);
    ws.did_open_file(src2_loc);

    std::vector<fade_message_s> fms;
    file_manager.retrieve_fade_messages(fms);

    EXPECT_EQ(collect_and_get_diags_size(ws, file_manager), (size_t)0);
    EXPECT_EQ(fms.size(), GetParam().expected_fms);
    if (GetParam().expected_fms > 0)
    {
        EXPECT_TRUE(matches_message_codes(fms, { "F_IN001" }));
        EXPECT_TRUE(matches_faded_line_ranges(fms, { { 2, 2 } }));
    }
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

         END)";
    changes.clear();
    changes.push_back(document_change(new_f1_text.c_str(), new_f1_text.size()));
    ws_mngr.did_change_file("test/library/test_wks/file_1", 5, changes.data(), 1);
    EXPECT_EQ(consumer.diags.diagnostics_size(), static_cast<size_t>(0));
    EXPECT_EQ(consumer.fms.size(), static_cast<size_t>(0));
}
