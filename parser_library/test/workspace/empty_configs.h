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

#ifndef HLASMPLUGIN_PARSERLIBRARY_TEST_EMPTY_CONFIGS_H
#define HLASMPLUGIN_PARSERLIBRARY_TEST_EMPTY_CONFIGS_H

#include <memory>
#include <string>

#include "nlohmann/json_fwd.hpp"
#include "utils/path.h"
#include "utils/resource_location.h"

inline const auto pgm_conf_name = hlasm_plugin::utils::resource::resource_location(".hlasmplugin/pgm_conf.json");
inline const auto proc_grps_name = hlasm_plugin::utils::resource::resource_location(".hlasmplugin/proc_grps.json");
inline const auto b4g_conf_name = hlasm_plugin::utils::resource::resource_location("SYS/SUB/ASMPGM/.bridge.json");
inline const std::string empty_pgm_conf = R"({ "pgms": []})";
inline const std::string empty_proc_grps = R"({ "pgroups": []})";
inline const std::string empty_b4g_conf = R"({})";

std::shared_ptr<const nlohmann::json> make_empty_shared_json();

#endif // !HLASMPLUGIN_PARSERLIBRARY_TEST_EMPTY_CONFIGS_H
