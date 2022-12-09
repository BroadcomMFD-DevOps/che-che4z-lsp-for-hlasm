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

#ifndef HLASMPLUGIN_PARSERLIBRARY_DEBUG_LIB_PROVIDER_H
#define HLASMPLUGIN_PARSERLIBRARY_DEBUG_LIB_PROVIDER_H

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "utils/resource_location.h"
#include "workspaces/parse_lib_provider.h"

namespace hlasm_plugin::parser_library::workspaces {
class library;
class workspace;
} // namespace hlasm_plugin::parser_library::workspaces

namespace hlasm_plugin::parser_library::debugging {

// Implements dependency (macro and COPY files) fetcher for macro tracer.
// Takes the information from a workspace, but calls special methods for
// parsing that do not collide with LSP.
class debug_lib_provider final : public workspaces::parse_lib_provider
{
    std::unordered_map<utils::resource::resource_location, std::string, utils::resource::resource_location_hasher>
        files;
    std::vector<std::shared_ptr<workspaces::library>> libraries;
    std::atomic<bool>* cancel;

public:
    debug_lib_provider(std::vector<std::shared_ptr<workspaces::library>> libraries, std::atomic<bool>* cancel);

    workspaces::parse_result parse_library(
        const std::string& library, analyzing_context ctx, workspaces::library_data data) override;

    bool has_library(const std::string& library, const utils::resource::resource_location& program) const override;

    std::optional<std::pair<std::string, utils::resource::resource_location>> get_library(
        const std::string& library, const utils::resource::resource_location& program) const override;
};

} // namespace hlasm_plugin::parser_library::debugging

#endif
