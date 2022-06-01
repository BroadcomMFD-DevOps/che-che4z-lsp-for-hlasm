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

#include <filesystem>
#include <regex>

#include "network/uri/uri.hpp"

#include "utils/path.h"
#include "utils/path_conversions.h"
#include "utils/platform.h"

namespace hlasm_plugin::utils::resource {

struct dissected_uri
{
    std::optional<std::string> scheme = std::nullopt;
    std::optional<std::string> auth = std::nullopt;
    std::optional<std::string> path = std::nullopt;
    std::optional<std::string> query = std::nullopt;
    std::optional<std::string> fragment = std::nullopt;
};

std::string get_file_path(const network::uri& u)
{
    if (!u.has_path())
        return "";

    network::string_view path = u.path();

    if (utils::platform::is_windows())
    {
        // we get path always beginning with / on windows, e.g. /c:/Users/path
        path.remove_prefix(1);
    }

    std::string auth_path = path.to_string();
    if (utils::platform::is_windows())
    {
        auth_path[0] = (char)tolower((unsigned char)auth_path[0]);
    }

    return utils::path::lexically_normal(network::detail::decode(auth_path)).string();
}

dissected_uri dissect_uri(const std::string& uri)
{
    dissected_uri ret;

    try
    {
        network::uri u(uri);

        if (u.has_scheme() && u.scheme().to_string() == "file"
            && (!u.has_authority() || u.authority().to_string() == ""))
        {
            ret.path = get_file_path(u);
            return ret;
        }

        if (u.has_scheme())
            ret.scheme = u.scheme().to_string();
        if (u.has_authority())
            ret.auth = u.authority().to_string();
        if (u.has_path())
            ret.path = u.path().to_string();
        if (u.has_query())
            ret.query = u.query().to_string();
        if (u.has_fragment())
            ret.fragment = u.fragment().to_string();

        return ret;
    }
    catch (const std::exception&)
    {
        return ret;
    }
}

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

std::string resource_location::to_presentable(bool human_readable_only) const
{
    dissected_uri dis_uri = dissect_uri(m_uri);
    std::string s = "";

    if (dis_uri.scheme.has_value())
        s.append("Scheme: ").append(dis_uri.scheme.value()).append("\n");
    if (dis_uri.auth.has_value())
        s.append("Authority: ").append(dis_uri.auth.value()).append("\n");
    if (dis_uri.path.has_value())
        s.append("Path: ").append(dis_uri.path.value()).append("\n");
    if (dis_uri.query.has_value())
        s.append("Query: ").append(dis_uri.query.value()).append("\n");
    if (dis_uri.fragment.has_value())
        s.append("Fragment: ").append(dis_uri.fragment.value()).append("\n");

    if (!human_readable_only)
        s.append("Raw URI: ").append(m_uri);

    if (s.ends_with("\n"))
        return s.substr(0, s.size() - 1);

    return s;
}

resource_location resource_location::join(const resource_location& rl, std::string_view relative_path)
{
    std::string uri = rl.get_uri();

    if (!rl.m_uri.empty() && rl.m_uri.back() != '/')
        return resource_location(uri.append("/").append(relative_path));

    return resource_location(uri.append(relative_path));
}

std::size_t resource_location_hasher::operator()(const resource_location& rl) const
{
    return std::hash<std::string> {}(rl.get_uri());
}
} // namespace hlasm_plugin::utils::resource