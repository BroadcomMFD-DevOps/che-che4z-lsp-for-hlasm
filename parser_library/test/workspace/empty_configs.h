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

#include <filesystem>

#include "utils/path.h"
#include "utils/resource_location.h"

inline auto pgm_conf_name = hlasm_plugin::utils::resource::resource_location(".hlasmplugin/pgm_conf.json");
inline auto proc_grps_name = hlasm_plugin::utils::resource::resource_location(".hlasmplugin/proc_grps.json");
inline std::string empty_pgm_conf = R"({ "pgms": []})";
inline std::string empty_proc_grps = R"({ "pgroups": []})";

#endif // !HLASMPLUGIN_PARSERLIBRARY_TEST_EMPTY_CONFIGS_H
