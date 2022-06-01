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

#ifndef HLASMPLUGIN_UTILS_CONTENT_LOADER_H
#define HLASMPLUGIN_UTILS_CONTENT_LOADER_H

#include <optional>
#include <string>
#include <unordered_map>

#include "resource_location.h"
#include "utils/general_hashers.h"
#include "utils/list_directory_rc.h"

namespace hlasm_plugin::utils::resource {

using list_directory_result = std::pair<
    std::unordered_map<std::string, utils::resource::resource_location, utils::hashers::string_hasher, std::equal_to<>>,
    utils::path::list_directory_rc>;

class content_loader
{
public:
    virtual std::optional<std::string> load_text(const resource_location& res_loc) const = 0;
    virtual list_directory_result list_directory_files(
        const utils::resource::resource_location& directory_loc) const = 0;
    virtual std::string filename(const utils::resource::resource_location& res_loc) const = 0;

protected:
    ~content_loader() = default;
};

std::optional<std::string> load_text(const resource_location& res_loc);
list_directory_result list_directory_files(const utils::resource::resource_location& directory_loc);
std::string filename(const utils::resource::resource_location& res_loc);

} // namespace hlasm_plugin::utils::resource

#endif