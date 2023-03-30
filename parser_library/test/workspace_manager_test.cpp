/*
 * Copyright (c) 2019 Broadcom.
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

#include "common_testing.h"
#include "lib_config.h"
#include "message_consumer_mock.h"
#include "nlohmann/json.hpp"
#include "utils/path.h"
#include "utils/platform.h"
#include "workspace/consume_diagnostics_mock.h"
#include "workspace_manager.h"

using namespace hlasm_plugin::parser_library;

TEST(workspace_manager, add_not_existing_workspace)
{
    workspace_manager ws_mngr;
    diag_consumer_mock consumer;
    ws_mngr.register_diagnostics_consumer(&consumer);

    ws_mngr.add_workspace("workspace", "not_existing");

    size_t count = ws_mngr.get_workspaces_count();
    EXPECT_EQ(count, (size_t)1);

    EXPECT_EQ(consumer.diags.diagnostics_size(), (size_t)0);
}

TEST(workspace_manager, add_existing_workspace)
{
    workspace_manager ws_mngr;
    diag_consumer_mock consumer;
    ws_mngr.register_diagnostics_consumer(&consumer);

    ws_mngr.add_workspace("workspace", "test/library/test_wks");

    size_t count = ws_mngr.get_workspaces_count();
    EXPECT_EQ(count, (size_t)1);

    EXPECT_EQ(consumer.diags.diagnostics_size(), (size_t)0);
}

TEST(workspace_manager, did_open_file)
{
    workspace_manager ws_mngr;
    diag_consumer_mock consumer;
    ws_mngr.register_diagnostics_consumer(&consumer);

    ws_mngr.add_workspace("workspace", "test/library/test_wks");

    std::string input_text = "label lr 1,2";
    ws_mngr.did_open_file("test/library/test_wks/some_file", 1, input_text.c_str(), input_text.size());
    EXPECT_FALSE(ws_mngr.idle_handler());

    EXPECT_EQ(consumer.diags.diagnostics_size(), (size_t)0);
}

TEST(workspace_manager, did_change_file)
{
    workspace_manager ws_mngr;
    diag_consumer_mock consumer;
    ws_mngr.register_diagnostics_consumer(&consumer);

    ws_mngr.add_workspace("workspace", "test/library/test_wks");
    std::string input = "label lr 1,2 remark";
    ws_mngr.did_open_file("test/library/test_wks/new_file", 1, input.c_str(), input.size());
    EXPECT_FALSE(ws_mngr.idle_handler());

    EXPECT_EQ(consumer.diags.diagnostics_size(), (size_t)0);

    std::vector<document_change> changes;
    std::string new_text = "anop";
    changes.push_back(document_change({ { 0, 6 }, { 0, input.size() } }, new_text.c_str(), new_text.size()));

    ws_mngr.did_change_file("test/library/test_wks/new_file", 2, changes.data(), 1);
    EXPECT_FALSE(ws_mngr.idle_handler());

    EXPECT_EQ(consumer.diags.diagnostics_size(), (size_t)1);

    std::vector<document_change> changes1;
    std::string new_text1 = "";
    changes1.push_back(document_change({ { 0, 6 }, { 0, 10 } }, new_text1.c_str(), new_text1.size()));

    ws_mngr.did_change_file("test/library/test_wks/new_file", 3, changes1.data(), 1);
    EXPECT_FALSE(ws_mngr.idle_handler());

    EXPECT_GT(consumer.diags.diagnostics_size(), (size_t)0);
}
