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

struct file_manager_impl_test : public file_manager_impl
{
    list_directory_result list_directory_files(const resource_location& directory) const override
    {
        list_directory_result result;

        for (const auto& [key, _] : get_files())
        {
            auto rel_path = key.lexically_relative(directory);
            if (rel_path.get_uri().empty() || rel_path.lexically_out_of_scope())
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
            if (rel_path.get_uri().empty() || rel_path.lexically_out_of_scope())
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

    file_manager.did_open_file(resource_location(".hlasmplugin/proc_grps.json"),
        1,
        R"({"pgroups":[{"name":"P1","libs":[{"path":"ASMMACP1","use_alternate_root":true},"ASMMACP1"]},{"name":"P2","libs":[{"path":"ASMMACP2","use_alternate_root":true},"ASMMACP2"]}]})");
    file_manager.did_open_file(resource_location("SYS/SUB/ASMPGM/.bridge.json"),
        1,
        R"({"elements":{"A":{"processorGroup":"P1"}},"defaultProcessorGroup":"P2","fileExtension":""})");
    file_manager.did_open_file(resource_location("SYS/SUB/ASMMACP1/MAC1"),
        1,
        R"(        MACRO
        MAC1
        MNOTE 4,'SYS/SUB/ASMMACP1/MAC1'
        MEND
)");
    file_manager.did_open_file(resource_location("SYS/SUB/ASMMACP2/MAC1"),
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
    file_manager.did_open_file(resource_location("SYS/SUB/ASMPGM/A"),
        1,
        R"(
        MAC1
        MAC2
)");
    file_manager.did_open_file(resource_location("SYS/SUB/ASMPGM/B"),
        1,
        R"(
        MAC1
        MAC2
)");

    lib_config config;
    workspace::shared_json global_settings = make_empty_shared_json();
    workspace ws(resource_location(), "workspace_name", file_manager, config, global_settings);
    ws.open();

    ws.did_open_file(resource_location("SYS/SUB/ASMPGM/A"));
    ws.collect_diags();
    file_manager.collect_diags();

    EXPECT_TRUE(ws.diags().empty());
    EXPECT_TRUE(matches_message_text(file_manager.diags(), { "SYS/SUB/ASMMACP1/MAC1", "ASMMACP1/MAC2" }));

    ws.did_close_file(resource_location("SYS/SUB/ASMPGM/A"));
    file_manager.diags().clear();

    ws.did_open_file(resource_location("SYS/SUB/ASMPGM/B"));
    ws.collect_diags();
    file_manager.collect_diags();

    EXPECT_TRUE(ws.diags().empty());
    EXPECT_TRUE(matches_message_text(file_manager.diags(), { "SYS/SUB/ASMMACP2/MAC1", "ASMMACP2/MAC2" }));

    ws.did_close_file(resource_location("SYS/SUB/ASMPGM/B"));
}