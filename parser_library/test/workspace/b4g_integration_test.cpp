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
#include "empty_configs.h"
#include "utils/list_directory_rc.h"
#include "utils/resource_location.h"
#include "workspaces/file_manager_impl.h"
#include "workspaces/workspace.h"

using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::parser_library::workspaces;
using hlasm_plugin::utils::resource::resource_location;

namespace {
const resource_location pgm_a("SYS/SUB/ASMPGM/A");
const resource_location pgm_b("SYS/SUB/ASMPGM/B");
const resource_location mac1("SYS/SUB/ASMMAC/MAC1");
const resource_location mac1_p1("SYS/SUB/ASMMACP1/MAC1");
const resource_location mac1_p2("SYS/SUB/ASMMACP2/MAC1");

void change_and_reparse(file_manager& fm, workspace& ws, const resource_location& rl, std::string_view new_content)
{
    static size_t version = 2;
    document_change doc_change(new_content.data(), new_content.size());
    fm.did_change_file(rl, version++, &doc_change, 1);
    ws.did_change_file(rl, &doc_change, 1);
    parse_all_files(ws);
}
} // namespace

struct file_manager_impl_test : public file_manager_impl
{
    list_directory_result list_directory_files(const resource_location& directory) const override
    {
        list_directory_result result;

        for (const auto& [key, _] : get_files())
        {
            auto rel_path = key.lexically_relative(directory);
            if (rel_path.empty() || rel_path.lexically_out_of_scope())
                continue;
            auto first_filename = rel_path.get_uri();
            if (auto n = first_filename.find('/'); n != std::string::npos)
                continue;

            result.first.push_back(std::make_pair(first_filename, resource_location::join(directory, first_filename)));
        }

        result.second = hlasm_plugin::utils::path::list_directory_rc::done;

        return result;
    }
    list_directory_result list_directory_subdirs_and_symlinks(const resource_location& directory) const override
    {
        list_directory_result result;

        for (const auto& [key, _] : get_files())
        {
            auto rel_path = key.lexically_relative(directory);
            if (rel_path.empty() || rel_path.lexically_out_of_scope())
                continue;
            auto first_filename = rel_path.get_uri();
            if (auto n = first_filename.find('/'); n != std::string::npos)
                first_filename.erase(n);
            else
                continue;

            result.first.push_back(std::make_pair(first_filename, resource_location::join(directory, first_filename)));
        }

        result.second = hlasm_plugin::utils::path::list_directory_rc::done;

        return result;
    }
};

TEST(b4g_integration_test, basic_pgm_conf_retrieval)
{
    file_manager_impl_test file_manager;

    file_manager.did_open_file(proc_grps_name,
        1,
        R"({"pgroups":[{"name":"P1","libs":[{"path":"ASMMACP1","prefer_alternate_root":true},"ASMMACP1"]},{"name":"P2","libs":[{"path":"ASMMACP2","prefer_alternate_root":true},"ASMMACP2"]}]})");
    file_manager.did_open_file(b4g_conf_name,
        1,
        R"({"elements":{"A":{"processorGroup":"P1"}},"defaultProcessorGroup":"P2","fileExtension":""})");
    file_manager.did_open_file(mac1_p1,
        1,
        R"(        MACRO
        MAC1
        MNOTE 4,'SYS/SUB/ASMMACP1/MAC1'
        MEND
)");
    file_manager.did_open_file(mac1_p2,
        1,
        R"(        MACRO
        MAC1
        MNOTE 4,'SYS/SUB/ASMMACP2/MAC1'
        MEND
)");
    file_manager.did_open_file(resource_location("ASMMACP1/MAC2"),
        1,
        R"(        MACRO
        MAC2
        MNOTE 4,'ASMMACP1/MAC2'
        MEND
)");
    file_manager.did_open_file(resource_location("ASMMACP2/MAC2"),
        1,
        R"(        MACRO
        MAC2
        MNOTE 4,'ASMMACP2/MAC2'
        MEND
)");
    file_manager.did_open_file(pgm_a,
        1,
        R"(
        MAC1
        MAC2
)");
    file_manager.did_open_file(pgm_b,
        1,
        R"(
        MAC1
        MAC2
)");

    lib_config config;
    shared_json global_settings = make_empty_shared_json();
    workspace ws(resource_location(), "workspace_name", file_manager, config, global_settings);
    ws.open();

    ws.did_open_file(pgm_a);
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_text(ws.diags(), { "SYS/SUB/ASMMACP1/MAC1", "ASMMACP1/MAC2" }));
    ws.diags().clear();

    ws.did_close_file(pgm_a);
    parse_all_files(ws);

    ws.did_open_file(pgm_b);
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_text(ws.diags(), { "SYS/SUB/ASMMACP2/MAC1", "ASMMACP2/MAC2" }));

    ws.did_close_file(pgm_b);
}

TEST(b4g_integration_test, invalid_bridge_json)
{
    file_manager_impl_test file_manager;

    file_manager.did_open_file(proc_grps_name, 1, empty_proc_grps);
    file_manager.did_open_file(b4g_conf_name, 1, empty_b4g_conf);
    file_manager.did_open_file(pgm_a, 1, "");

    lib_config config;
    shared_json global_settings = make_empty_shared_json();
    workspace ws(resource_location(), "workspace_name", file_manager, config, global_settings);
    ws.open();

    ws.did_open_file(pgm_a);
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G001" }));
}

TEST(b4g_integration_test, missing_pgroup)
{
    file_manager_impl_test file_manager;

    file_manager.did_open_file(proc_grps_name, 1, empty_proc_grps);
    file_manager.did_open_file(b4g_conf_name,
        1,
        R"({"elements":{"A":{"processorGroup":"P1"}},"defaultProcessorGroup":"P2","fileExtension":""})");
    file_manager.did_open_file(pgm_a, 1, "");

    lib_config config;
    shared_json global_settings = make_empty_shared_json();
    workspace ws(resource_location(), "workspace_name", file_manager, config, global_settings);
    ws.open();

    ws.did_open_file(pgm_a);
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002", "B4G002" }));
}

TEST(b4g_integration_test, missing_pgroup_but_not_used)
{
    file_manager_impl_test file_manager;

    file_manager.did_open_file(proc_grps_name, 1, empty_proc_grps);
    file_manager.did_open_file(b4g_conf_name,
        1,
        R"({"elements":{"A":{"processorGroup":"P1"}},"defaultProcessorGroup":"P2","fileExtension":""})");
    file_manager.did_open_file(pgm_a, 1, "");

    lib_config config;
    shared_json global_settings = make_empty_shared_json();
    workspace ws(resource_location(), "workspace_name", file_manager, config, global_settings);
    ws.open();

    ws.did_open_file(pgm_a);
    parse_all_files(ws);
    ws.did_close_file(pgm_a);
    parse_all_files(ws);

    ws.collect_diags();

    EXPECT_TRUE(ws.diags().empty());
}

TEST(b4g_integration_test, bridge_config_changed)
{
    file_manager_impl_test file_manager;

    file_manager.did_open_file(
        proc_grps_name, 1, R"({"pgroups":[{"name":"P1","libs":[{"path":"ASMMAC","prefer_alternate_root":true}]}]})");
    file_manager.did_open_file(b4g_conf_name, 1, empty_b4g_conf);
    file_manager.did_open_file(pgm_a, 1, " MAC1");
    file_manager.did_open_file(mac1,
        1,
        R"(        MACRO
        MAC1
        MNOTE 4,'MACRO'
        MEND
)");

    lib_config config;
    shared_json global_settings = make_empty_shared_json();
    workspace ws(resource_location(), "workspace_name", file_manager, config, global_settings);
    ws.open();

    ws.did_open_file(pgm_a);
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "E049", "B4G001" }));

    ws.diags().clear();
    change_and_reparse(
        file_manager, ws, b4g_conf_name, R"({"elements":{},"defaultProcessorGroup":"P1","fileExtension":""})");
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "MNOTE" }));

    ws.diags().clear();

    change_and_reparse(file_manager, ws, b4g_conf_name, empty_b4g_conf);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "MNOTE", "B4G001" }));

    ws.diags().clear();

    change_and_reparse(file_manager, ws, pgm_a, " MAC1 ");
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "E049", "B4G001" }));
}

TEST(b4g_integration_test, proc_config_changed)
{
    file_manager_impl_test file_manager;

    file_manager.did_open_file(proc_grps_name, 1, empty_proc_grps);
    file_manager.did_open_file(b4g_conf_name, 1, R"({"elements":{},"defaultProcessorGroup":"P1","fileExtension":""})");
    file_manager.did_open_file(pgm_a, 1, " MAC1");
    file_manager.did_open_file(mac1,
        1,
        R"(        MACRO
        MAC1
        MNOTE 4,'MACRO'
        MEND
)");

    lib_config config;
    shared_json global_settings = make_empty_shared_json();
    workspace ws(resource_location(), "workspace_name", file_manager, config, global_settings);
    ws.open();

    ws.did_open_file(pgm_a);
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "E049", "B4G002" }));

    ws.diags().clear();

    change_and_reparse(file_manager,
        ws,
        proc_grps_name,
        R"({"pgroups":[{"name":"P1","libs":[{"path":"ASMMAC","prefer_alternate_root":true}]}]})");
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "MNOTE" }));
}

TEST(b4g_integration_test, only_default_proc_group_exists)
{
    file_manager_impl fm;
    fm.did_open_file(b4g_conf_name,
        0,
        R"({"elements":{"A":{"processorGroup":"MISSING"}},"defaultProcessorGroup":"P1","fileExtension":""})");
    fm.did_open_file(proc_grps_name, 1, R"({"pgroups":[{"name":"P1","libs":[]}]})");
    fm.did_open_file(pgm_a, 1, "");

    lib_config config;
    shared_json global_settings = make_empty_shared_json();
    workspace ws(fm, config, global_settings);
    ws.open();
    ws.did_open_file(pgm_a);
    parse_all_files(ws);

    ws.collect_diags();
    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002" }));

    ws.diags().clear();

    change_and_reparse(fm, ws, pgm_a, " ");
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002" }));
}

TEST(b4g_integration_test, b4g_conf_noproc_proc_group)
{
    file_manager_impl fm;
    fm.did_open_file(b4g_conf_name,
        0,
        R"({"elements":{"A":{"processorGroup":"*NOPROC*"}},"defaultProcessorGroup":"MISSING","fileExtension":""})");
    fm.did_open_file(proc_grps_name, 0, empty_proc_grps);
    fm.did_open_file(pgm_a, 1, "");

    lib_config config;
    shared_json global_settings = make_empty_shared_json();
    workspace ws(fm, config, global_settings);
    ws.open();
    ws.did_open_file(pgm_a);
    parse_all_files(ws);

    ws.collect_diags();
    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002" }));

    ws.diags().clear();

    change_and_reparse(fm, ws, pgm_a, " ");
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002" }));
}

TEST(b4g_integration_test, b4g_conf_noproc_proc_group_default)
{
    file_manager_impl fm;
    fm.did_open_file(b4g_conf_name,
        0,
        R"({"elements":{"A":{"processorGroup":"MISSING"}},"defaultProcessorGroup":"*NOPROC*","fileExtension":""})");
    fm.did_open_file(proc_grps_name, 0, empty_proc_grps);
    fm.did_open_file(pgm_a, 1, "");

    lib_config config;
    shared_json global_settings = make_empty_shared_json();
    workspace ws(fm, config, global_settings);
    ws.open();
    ws.did_open_file(pgm_a);
    parse_all_files(ws);

    ws.collect_diags();
    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002" }));

    ws.diags().clear();

    change_and_reparse(fm, ws, pgm_a, " ");
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002" }));
}
