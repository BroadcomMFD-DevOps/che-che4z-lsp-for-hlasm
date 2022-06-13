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

#include "utils/resource_location.h"

#include "utils/path_conversions.h"

namespace hlasm_plugin::utils::resource {

resource_location::resource_location(std::string uri)
    : m_uri(std::move(uri))
{}

resource_location::resource_location(std::string_view uri)
    : resource_location(std::string(uri))
{}

resource_location::resource_location(const char* uri)
    : resource_location(std::string(uri))
{}

const std::string& resource_location::get_uri() const { return m_uri; }

std::string resource_location::get_path() const { return m_uri.size() != 0 ? utils::path::uri_to_path(m_uri) : m_uri; }

std::string resource_location::to_presentable(bool debug) const
{
    return utils::path::get_presentable_uri(m_uri, debug);
}

void resource_location::to_directory()
{
    if (!m_uri.empty() && m_uri.back() != '/')
        m_uri.append("/");
}

bool resource_location::lexically_out_of_scope() const
{
    return m_uri == std::string_view("..") || m_uri.starts_with("../") || m_uri.starts_with("..\\");
}

void resource_location::join(std::string_view relative_path)
{
    if (auto f = m_uri.find_last_of("/:"); f != std::string::npos)
    {
        m_uri = m_uri.substr(0, f + 1);
        m_uri.append(relative_path);
    }
    else
        m_uri = relative_path;
}

resource_location resource_location::join(resource_location rl, std::string_view relative_path)
{
    rl.join(relative_path);

    return rl;
}

std::size_t resource_location_hasher::operator()(const resource_location& rl) const
{
    return std::hash<std::string> {}(rl.get_uri());
}
} // namespace hlasm_plugin::utils::resource