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

#include "utils/filesystem_content_loader.h"

#include <fstream>

#include "utils/utf8text.h"

namespace hlasm_plugin::utils::resource {

std::optional<std::string> filesystem_content_loader::load_text(const resource_location& resource) const
{
    std::ifstream fin(resource.get_path(), std::ios::in | std::ios::binary);
    std::string text;

    if (fin)
    {
        fin.seekg(0, std::ios::end);
        text.resize((size_t)fin.tellg());
        fin.seekg(0, std::ios::beg);
        fin.read(&text[0], text.size());
        fin.close();

        return text;
    }
    else
    {
        return std::nullopt;
    }
}

} // namespace hlasm_plugin::utils::resource