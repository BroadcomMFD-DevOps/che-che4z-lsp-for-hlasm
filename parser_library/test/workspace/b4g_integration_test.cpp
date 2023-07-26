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

#include <regex>
#include <string>
#include <string_view>

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
constexpr auto prepend_ws_loc = [](std::string path) {
    static const resource_location rl("scheme://ws/");
    return resource_location::join(rl, path).lexically_normal();
};

const std::string empty_b4g_conf(R"({})");
const auto ws_rl = prepend_ws_loc("");
const auto proc_grps_rl = prepend_ws_loc(proc_grps_name.get_uri());
const auto pgm_conf_rl = prepend_ws_loc(pgm_conf_name.get_uri());
const auto b4g_conf_rl = prepend_ws_loc("SYS/SUB/ASMPGM/.bridge.json");
const auto pgm_a = prepend_ws_loc("SYS/SUB/ASMPGM/A");
const auto pgm_b = prepend_ws_loc("SYS/SUB/ASMPGM/B");
const auto pgm_a_diff_path = prepend_ws_loc("PGMS/A");
const auto pgm_dollars = prepend_ws_loc("SYS/SUB/ASMPGM/$$$");
const auto sys_sub_p1_mac1 = prepend_ws_loc("SYS/SUB/ASMMACP1/MAC1");
const auto sys_sub_p2_mac1 = prepend_ws_loc("SYS/SUB/ASMMACP2/MAC1");
const auto sys_sub_p3_mac1 = prepend_ws_loc("SYS/SUB/ASMMACP3/MAC1");
const auto p1_mac2 = prepend_ws_loc("ASMMACP1/MAC2");
const auto p2_mac2 = prepend_ws_loc("ASMMACP2/MAC2");
const auto p3_mac2 = prepend_ws_loc("ASMMACP3/MAC2");

std::string macro_template(R"(        MACRO
        MAC$x
        MNOTE 4,'$y'
        MEND
)");

std::string get_macro_content(std::string mac_template, std::string mac_id, std::string mac_path)
{
    return std::regex_replace(
        std::regex_replace(mac_template, std::regex("\\$x"), mac_id), std::regex("\\$y"), mac_path);
}

void change_and_reparse(file_manager& fm, workspace& ws, const resource_location& rl, std::string_view new_content)
{
    static size_t version = 2;
    document_change doc_change(new_content.data(), new_content.size());
    fm.did_change_file(rl, version++, &doc_change, 1);
    run_if_valid(ws.did_change_file(rl, file_content_state::changed_content));
    parse_all_files(ws);
}


struct file_manager_impl_test : public file_manager_impl
{
    hlasm_plugin::utils::value_task<list_directory_result> list_directory_files(
        const resource_location& directory) const override
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

        return hlasm_plugin::utils::value_task<list_directory_result>::from_value(std::move(result));
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

class workspace_test : public workspace
{
public:
    workspace_test(file_manager& fm)
        : workspace(ws_rl, "workspace_name", fm, m_config, m_global_settings)
    {
        open().run();
    }

private:
    lib_config m_config;
    shared_json m_global_settings = make_empty_shared_json();
};

} // namespace

TEST(b4g_integration_test, basic_pgm_conf_retrieval)
{
    file_manager_impl_test fm;

    fm.did_open_file(proc_grps_rl,
        1,
        R"({"pgroups":[{"name":"P1","libs":[{"path":"ASMMACP1","prefer_alternate_root":true},"ASMMACP1"]},{"name":"P2","libs":[{"path":"ASMMACP2","prefer_alternate_root":true},"ASMMACP2"]},{"name":"P3","libs":[{"path":"ASMMACP3","prefer_alternate_root":true},"ASMMACP3"]}]})");
    fm.did_open_file(b4g_conf_rl,
        1,
        R"({"elements":{"A":{"processorGroup":"P1"},"$$$":{"processorGroup":"P3"}},"defaultProcessorGroup":"P2","fileExtension":""})");
    fm.did_open_file(sys_sub_p1_mac1, 1, get_macro_content(macro_template, "1", sys_sub_p1_mac1.get_uri()));
    fm.did_open_file(sys_sub_p2_mac1, 1, get_macro_content(macro_template, "1", sys_sub_p2_mac1.get_uri()));
    fm.did_open_file(sys_sub_p3_mac1, 1, get_macro_content(macro_template, "1", sys_sub_p3_mac1.get_uri()));
    fm.did_open_file(p1_mac2, 1, get_macro_content(macro_template, "2", p1_mac2.get_uri()));
    fm.did_open_file(p2_mac2, 1, get_macro_content(macro_template, "2", p2_mac2.get_uri()));
    fm.did_open_file(p3_mac2, 1, get_macro_content(macro_template, "2", p3_mac2.get_uri()));

    std::string pgm_template(R"(
        MAC1
        MAC2
)");

    fm.did_open_file(pgm_a, 1, pgm_template);
    fm.did_open_file(pgm_b, 1, pgm_template);
    fm.did_open_file(pgm_dollars, 1, pgm_template);

    workspace_test ws(fm);

    const auto check_mnote = [&ws](const resource_location& pgm, std::initializer_list<std::string> mnote_locations) {
        run_if_valid(ws.did_open_file(pgm));
        parse_all_files(ws);
        ws.collect_diags();

        auto match = matches_message_text(ws.diags(), mnote_locations);
        ws.diags().clear();

        run_if_valid(ws.did_close_file(pgm));
        parse_all_files(ws);

        return match;
    };

    EXPECT_TRUE(check_mnote(pgm_a, { sys_sub_p1_mac1.get_uri(), p1_mac2.get_uri() }));
    EXPECT_TRUE(check_mnote(pgm_b, { sys_sub_p2_mac1.get_uri(), p2_mac2.get_uri() }));
    EXPECT_TRUE(check_mnote(pgm_dollars, { sys_sub_p3_mac1.get_uri(), p3_mac2.get_uri() }));
}

class pgm_conf_preference_helper
{
public:
    file_manager_impl_test fm;
    lib_config config;
    shared_json global_settings = make_empty_shared_json();
    workspace ws = workspace(ws_rl, "workspace_name", fm, config, global_settings);

    const std::string pgm_conf_template = R"({
  "pgms": [
    {
      "program": "SYS/SUB/ASMPGM/$x",
      "pgroup": "P1"
    }
  ]
})";

    pgm_conf_preference_helper(std::string pgm_name)
    {
        fm.did_open_file(proc_grps_rl,
            1,
            R"({"pgroups":[{"name":"P1","libs":["SYS/SUB/ASMMACP1"]},{"name":"P2","libs":["SYS/SUB/ASMMACP2"]},{"name":"P3","libs":["SYS/SUB/ASMMACP3"]}]})");

        fm.did_open_file(pgm_conf_rl, 0, std::regex_replace(pgm_conf_template, std::regex("\\$x"), pgm_name));
        fm.did_open_file(b4g_conf_rl,
            0,
            R"({"elements":{"A":{"processorGroup":"P2"},"B":{"processorGroup":"P2"}},"defaultProcessorGroup":"P3","fileExtension":""})");

        fm.did_open_file(pgm_a, 1, " MAC1");
        fm.did_open_file(pgm_b, 1, " MAC1");
        fm.did_open_file(sys_sub_p1_mac1, 1, get_macro_content(macro_template, "1", sys_sub_p1_mac1.get_uri()));
        fm.did_open_file(sys_sub_p2_mac1, 1, get_macro_content(macro_template, "1", sys_sub_p2_mac1.get_uri()));
        fm.did_open_file(sys_sub_p3_mac1, 1, get_macro_content(macro_template, "1", sys_sub_p3_mac1.get_uri()));

        ws.open().run();
    }
};

TEST(b4g_integration_test, pgm_conf_preference_exact_path)
{
    pgm_conf_preference_helper helper("A");
    auto& ws = helper.ws;

    run_if_valid(ws.did_open_file(pgm_b));
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_text(ws.diags(), { sys_sub_p2_mac1.get_uri() }));

    ws.diags().clear();

    run_if_valid(ws.did_open_file(pgm_a));
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_text(ws.diags(), { sys_sub_p1_mac1.get_uri(), sys_sub_p2_mac1.get_uri() }));
}

TEST(b4g_integration_test, pgm_conf_preference_regex_path)
{
    pgm_conf_preference_helper helper("A");
    auto& ws = helper.ws;
    auto& fm = helper.fm;

    run_if_valid(ws.did_open_file(pgm_b));
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_text(ws.diags(), { sys_sub_p2_mac1.get_uri() }));

    ws.diags().clear();
    change_and_reparse(fm, ws, pgm_conf_rl, std::regex_replace(helper.pgm_conf_template, std::regex("\\$x"), "*"));

    run_if_valid(ws.did_open_file(pgm_a));
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_text(ws.diags(), { sys_sub_p1_mac1.get_uri(), sys_sub_p1_mac1.get_uri() }));
}

TEST(b4g_integration_test, invalid_bridge_json)
{
    file_manager_impl_test fm;

    fm.did_open_file(proc_grps_rl, 1, empty_proc_grps);
    fm.did_open_file(b4g_conf_rl, 1, empty_b4g_conf);
    fm.did_open_file(pgm_a, 1, "");

    workspace_test ws(fm);

    run_if_valid(ws.did_open_file(pgm_a));
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G001" }));
}

TEST(b4g_integration_test, missing_pgroup)
{
    file_manager_impl_test fm;

    fm.did_open_file(proc_grps_rl, 1, empty_proc_grps);
    fm.did_open_file(b4g_conf_rl,
        1,
        R"({"elements":{"A":{"processorGroup":"P1"}},"defaultProcessorGroup":"P2","fileExtension":""})");
    fm.did_open_file(pgm_a, 1, "");

    workspace_test ws(fm);

    run_if_valid(ws.did_open_file(pgm_a));
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002" }));

    ws.toggle_non_critical_configuration_diagnostics();
    ws.diags().clear();
    ws.collect_diags();
    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002", "B4G003" }));
}

TEST(b4g_integration_test, missing_pgroup_but_not_used)
{
    file_manager_impl_test fm;

    fm.did_open_file(proc_grps_rl, 1, empty_proc_grps);
    fm.did_open_file(b4g_conf_rl,
        1,
        R"({"elements":{"A":{"processorGroup":"P1"}},"defaultProcessorGroup":"P2","fileExtension":""})");
    fm.did_open_file(pgm_a, 1, "");

    workspace_test ws(fm);

    run_if_valid(ws.did_open_file(pgm_a));
    parse_all_files(ws);
    run_if_valid(ws.did_close_file(pgm_a));
    parse_all_files(ws);

    ws.collect_diags();

    EXPECT_TRUE(ws.diags().empty());
}

TEST(b4g_integration_test, bridge_config_changed)
{
    file_manager_impl_test fm;

    fm.did_open_file(
        proc_grps_rl, 1, R"({"pgroups":[{"name":"P1","libs":[{"path":"ASMMACP1","prefer_alternate_root":true}]}]})");
    fm.did_open_file(b4g_conf_rl, 1, empty_b4g_conf);
    fm.did_open_file(pgm_a, 1, " MAC1");
    fm.did_open_file(sys_sub_p1_mac1, 1, get_macro_content(macro_template, "1", sys_sub_p1_mac1.get_uri()));

    workspace_test ws(fm);

    run_if_valid(ws.did_open_file(pgm_a));
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "E049", "B4G001" }));

    ws.diags().clear();
    change_and_reparse(fm, ws, b4g_conf_rl, R"({"elements":{},"defaultProcessorGroup":"P1","fileExtension":""})");
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "MNOTE" }));

    ws.diags().clear();

    change_and_reparse(fm, ws, b4g_conf_rl, empty_b4g_conf);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "MNOTE", "B4G001" }));

    ws.diags().clear();

    change_and_reparse(fm, ws, pgm_a, " MAC1 ");
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "E049", "B4G001" }));
}

TEST(b4g_integration_test, proc_config_changed)
{
    file_manager_impl_test fm;

    fm.did_open_file(proc_grps_rl, 1, empty_proc_grps);
    fm.did_open_file(b4g_conf_rl, 1, R"({"elements":{},"defaultProcessorGroup":"P1","fileExtension":""})");
    fm.did_open_file(pgm_a, 1, " MAC1");
    fm.did_open_file(sys_sub_p1_mac1, 1, get_macro_content(macro_template, "1", sys_sub_p1_mac1.get_uri()));

    workspace_test ws(fm);

    run_if_valid(ws.did_open_file(pgm_a));
    parse_all_files(ws);
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "E049", "B4G002" }));

    ws.diags().clear();

    change_and_reparse(fm,
        ws,
        proc_grps_rl,
        R"({"pgroups":[{"name":"P1","libs":[{"path":"ASMMACP1","prefer_alternate_root":true}]}]})");
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "MNOTE" }));
}

TEST(b4g_integration_test, only_default_proc_group_exists)
{
    file_manager_impl fm;
    fm.did_open_file(b4g_conf_rl,
        0,
        R"({"elements":{"A":{"processorGroup":"MISSING"}},"defaultProcessorGroup":"P1","fileExtension":""})");
    fm.did_open_file(proc_grps_rl, 1, R"({"pgroups":[{"name":"P1","libs":[]}]})");
    fm.did_open_file(pgm_a, 1, "");

    workspace_test ws(fm);

    run_if_valid(ws.did_open_file(pgm_a));
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
    fm.did_open_file(b4g_conf_rl,
        0,
        R"({"elements":{"A":{"processorGroup":"*NOPROC*"}},"defaultProcessorGroup":"MISSING","fileExtension":""})");
    fm.did_open_file(proc_grps_rl, 0, empty_proc_grps);
    fm.did_open_file(pgm_a, 1, "");

    workspace_test ws(fm);
    run_if_valid(ws.did_open_file(pgm_a));
    parse_all_files(ws);

    ws.collect_diags();
    EXPECT_TRUE(ws.diags().empty());

    ws.diags().clear();

    change_and_reparse(fm, ws, pgm_a, " ");
    ws.collect_diags();

    EXPECT_TRUE(ws.diags().empty());

    ws.toggle_non_critical_configuration_diagnostics();
    ws.diags().clear();
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G003" }));
}

TEST(b4g_integration_test, b4g_conf_noproc_proc_group_default)
{
    file_manager_impl fm;
    fm.did_open_file(b4g_conf_rl,
        0,
        R"({"elements":{"A":{"processorGroup":"MISSING"}},"defaultProcessorGroup":"*NOPROC*","fileExtension":""})");
    fm.did_open_file(proc_grps_rl, 0, empty_proc_grps);
    fm.did_open_file(pgm_a, 1, "");

    workspace_test ws(fm);
    run_if_valid(ws.did_open_file(pgm_a));
    parse_all_files(ws);

    ws.collect_diags();
    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002" }));

    ws.diags().clear();

    change_and_reparse(fm, ws, pgm_a, " ");
    ws.collect_diags();

    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002" }));
}

TEST(b4g_integration_test, missing_proc_group_diags)
{
    file_manager_impl_test fm;

    fm.did_open_file(proc_grps_rl, 1, empty_proc_grps);
    fm.did_open_file(b4g_conf_rl,
        1,
        R"({"elements":{"A":{"processorGroup":"P1"}},"defaultProcessorGroup":"P2","fileExtension":""})");
    fm.did_open_file(pgm_a, 1, "");
    fm.did_open_file(pgm_b, 1, "");
    fm.did_open_file(pgm_a_diff_path, 1, "");

    workspace_test ws(fm);

    run_if_valid(ws.did_open_file(pgm_a));
    parse_all_files(ws);
    ws.collect_diags();
    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002" }));

    ws.toggle_non_critical_configuration_diagnostics();
    ws.diags().clear();
    ws.collect_diags();
    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002", "B4G003" }));

    run_if_valid(ws.did_close_file(pgm_a));
    ws.diags().clear();
    ws.collect_diags();
    EXPECT_TRUE(ws.diags().empty());

    ws.toggle_non_critical_configuration_diagnostics();
    ws.diags().clear();
    ws.collect_diags();
    EXPECT_TRUE(ws.diags().empty());

    run_if_valid(ws.did_open_file(pgm_b));
    parse_all_files(ws);
    ws.diags().clear();
    ws.collect_diags();
    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G002" }));

    run_if_valid(ws.did_close_file(pgm_b));
    parse_all_files(ws);
    ws.diags().clear();
    ws.collect_diags();
    EXPECT_TRUE(ws.diags().empty());

    run_if_valid(ws.did_open_file(pgm_a_diff_path));
    ws.diags().clear();
    ws.collect_diags();
    EXPECT_TRUE(ws.diags().empty());
}

TEST(b4g_integration_test, missing_proc_group_diags_wildcards)
{
    file_manager_impl_test fm;

    fm.did_open_file(proc_grps_rl, 1, R"({"pgroups":[{"name":"P1","libs":[]}]})");
    // Wildcards implicitly in default proc group
    fm.did_open_file(b4g_conf_rl,
        1,
        R"({"elements":{"A":{"processorGroup":"P1"}},"defaultProcessorGroup":"P2","fileExtension":""})");
    fm.did_open_file(pgm_a, 1, "");

    workspace_test ws(fm);

    run_if_valid(ws.did_open_file(pgm_a));
    parse_all_files(ws);
    ws.collect_diags();
    EXPECT_TRUE(ws.diags().empty());

    ws.toggle_non_critical_configuration_diagnostics();
    ws.diags().clear();
    ws.collect_diags();
    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G003" }));

    run_if_valid(ws.did_close_file(pgm_a));
    ws.diags().clear();
    ws.collect_diags();
    EXPECT_TRUE(ws.diags().empty());
}

TEST(b4g_integration_test, missing_proc_group_diags_wildcards_noproc)
{
    file_manager_impl_test fm;

    fm.did_open_file(proc_grps_rl, 1, empty_proc_grps);
    fm.did_open_file(b4g_conf_rl,
        1,
        R"({"elements":{"A":{"processorGroup":"*NOPROC*"}},"defaultProcessorGroup":"P2","fileExtension":""})");
    fm.did_open_file(pgm_a, 1, "");

    workspace_test ws(fm);

    run_if_valid(ws.did_open_file(pgm_a));
    parse_all_files(ws);
    ws.collect_diags();
    EXPECT_TRUE(ws.diags().empty());

    ws.toggle_non_critical_configuration_diagnostics();
    ws.diags().clear();
    ws.collect_diags();
    EXPECT_TRUE(matches_message_codes(ws.diags(), { "B4G003" }));

    run_if_valid(ws.did_close_file(pgm_a));
    ws.diags().clear();
    ws.collect_diags();
    EXPECT_TRUE(ws.diags().empty());
}
