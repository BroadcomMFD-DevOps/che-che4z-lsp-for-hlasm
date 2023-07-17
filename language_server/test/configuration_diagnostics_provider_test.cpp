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

#include "gmock/gmock.h"
#include "json_channel.h"

#include "configuration_diagnostics_provider.h"
#include "nlohmann/json.hpp"
#include "ws_mngr_mock.h"

using namespace hlasm_plugin::language_server;
using namespace hlasm_plugin::language_server::test;
using namespace ::testing;

TEST(configuration_diagnostics_provider, predicate)
{
    ws_mngr_mock ws_mngr;

    configuration_diagnostics_provider cdp(ws_mngr);
    auto pred = cdp.get_filtering_predicate();

    EXPECT_TRUE(pred(nlohmann::json { { "method", "toggle_non_critical_configuration_diagnostics" } }));
    EXPECT_FALSE(pred(nlohmann::json { { "method", "toggle_non_critical_configuration_diagnostics1" } }));
    EXPECT_FALSE(pred(nlohmann::json { { "method", "toggle_non_critical_configuration_diagnostic" } }));
    EXPECT_FALSE(pred(nlohmann::json { { "nested", { "method", "toggle_non_critical_configuration_diagnostics" } } }));
}

TEST(configuration_diagnostics_provider, toggle)
{
    NiceMock<ws_mngr_mock> ws_mngr;
    configuration_diagnostics_provider cdp(ws_mngr);

    EXPECT_CALL(ws_mngr, toggle_non_critical_configuration_diagnostics).Times(1);

    cdp.write(nlohmann::json {});
}
