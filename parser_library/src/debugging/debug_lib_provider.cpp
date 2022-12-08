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

#include "debug_lib_provider.h"

#include "analyzer.h"
#include "workspaces/library.h"
#include "workspaces/workspace.h"

namespace hlasm_plugin::parser_library::debugging {

debug_lib_provider::debug_lib_provider(
    std::vector<std::shared_ptr<workspaces::library>> libraries, std::atomic<bool>* cancel)
    : libraries(std::move(libraries))
    , cancel(cancel)
{}

workspaces::parse_result debug_lib_provider::parse_library(
    const std::string& library, analyzing_context ctx, workspaces::library_data data)
{
    for (auto&& lib : libraries)
    {
        auto found = lib->get_file_content(library);
        if (found.first.empty())
            continue;

        const auto& [location, content] = *files.insert(std::move(found)).first;
        analyzer a(content,
            analyzer_options {
                location,
                this,
                std::move(ctx),
                data,
                collect_highlighting_info::no,
            });
        a.analyze(cancel);
        return cancel == nullptr || !*cancel;
    }

    return false;
}

bool debug_lib_provider::has_library(const std::string& library, const utils::resource::resource_location&) const
{
    for (auto&& lib : libraries)
        if (lib->has_file(library))
            return true;
    return false;
}

std::optional<std::pair<std::string, utils::resource::resource_location>> debug_lib_provider::get_library(
    const std::string& library, const utils::resource::resource_location&) const
{
    for (auto&& lib : libraries)
    {
        auto&& [location, content] = lib->get_file_content(library);
        if (location.empty())
            continue;
        return std::pair(std::move(content), std::move(location));
    }
    return {};
}

} // namespace hlasm_plugin::parser_library::debugging
