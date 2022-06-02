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

#include "utils/path_conversions.h"

#include <optional>
#include <regex>

#include "network/uri/uri.hpp"

#include "utils/path.h"
#include "utils/platform.h"

namespace hlasm_plugin::utils::path {

std::string uri_to_path(const std::string& uri)
{
    try
    {
        network::uri u(uri);

        if (u.scheme().compare("file"))
            return "";
        if (!u.has_path())
            return "";

        std::string auth_path;
        if (u.has_authority() && u.authority().to_string() != "")
        {
            auth_path = u.authority().to_string() + u.path().to_string();
            if (utils::platform::is_windows())
            {
                // handle remote locations correctly, like \\server\path
                auth_path = "//" + auth_path;
            }
        }
        else
        {
            network::string_view path = u.path();

            if (utils::platform::is_windows())
            {
                // we get path always beginning with / on windows, e.g. /c:/Users/path
                path.remove_prefix(1);
            }
            auth_path = path.to_string();

            if (utils::platform::is_windows())
            {
                auth_path[0] = (char)tolower((unsigned char)auth_path[0]);
            }
        }

        return utils::path::lexically_normal(network::detail::decode(auth_path)).string();
    }
    catch (const std::exception&)
    {
        return uri;
    }
}

// one letter schemas are valid, but Windows paths collide
const std::regex uri_like("^[A-Za-z][A-Za-z0-9+\\-.]+:");

std::string path_to_uri(std::string_view path)
{
    if (std::regex_search(path.begin(), path.end(), uri_like))
        return std::string(path);

    // network::detail::encode_path(uri) ignores @, which is incompatible with VS Code
    std::string uri;
    auto out = std::back_inserter(uri);

    for (char c : path)
    {
        if (c == '\\')
            c = '/';
        network::detail::encode_char(c, out, "/.%;=");
    }

    if (utils::platform::is_windows())
    {
        // in case of remote address such as \\server\path\to\file
        if (uri.size() >= 2 && uri[0] == '/' && uri[1] == '/')
            uri.insert(0, "file:");
        else
            uri.insert(0, "file:///");
    }
    else
    {
        uri.insert(0, "file://");
    }

    return uri;
}

namespace {
struct dissected_uri
{
    std::optional<std::string> scheme = std::nullopt;
    std::optional<std::string> host = std::nullopt;
    std::optional<std::string> auth = std::nullopt;
    std::optional<std::string> path = std::nullopt;
    std::optional<std::string> query = std::nullopt;
    std::optional<std::string> fragment = std::nullopt;
};

void format_path_pre_processing(std::string& hostname, std::string& path)
{
    if (!hostname.empty() && !path.empty() && path[0] == '/')
        path = path.substr(1, path.size() - 1);
}

void format_path_post_processing_win(std::string hostname, std::string& path)
{
    if (!path.empty() && hostname.empty())
        path.erase(0, 1);
}

std::string format_path(std::string hostname, std::string path)
{
    format_path_pre_processing(hostname, path);

    std::string s = utils::path::lexically_normal(utils::path::join(hostname, path)).string();

    if (utils::platform::is_windows())
        format_path_post_processing_win(hostname, s);

    return network::detail::decode(s);
}

std::string to_presentable_internal(const dissected_uri& dis_uri)
{
    std::string s;
    if (dis_uri.scheme.has_value())
        s.append(dis_uri.scheme.value()).append(":");
    if (dis_uri.host.has_value())
        s.append(dis_uri.host.value());
    if (dis_uri.path.has_value())
        s.append(dis_uri.path.value());

    return s;
}

std::string to_presentable_internal_debug(const dissected_uri& dis_uri, std::string_view raw_uri)
{
    std::string s;
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

    s.append("Raw URI: ").append(raw_uri);

    return s;
}

dissected_uri dissect_uri(const std::string& uri, bool debug)
{
    dissected_uri ret;

    try
    {
        network::uri u(uri);

        if (u.has_scheme() && u.scheme().to_string() == "file")
        {
            std::string h;
            std::string p;

            if (u.has_host())
                h = u.host().to_string();
            if (u.has_path())
                p = u.path().to_string();
            if (!h.empty() && u.has_authority())
                h.insert(0, "//");

            ret.path = format_path(h, p);

            return ret;
        }

        if (u.has_scheme())
            ret.scheme = u.scheme().to_string();
        if (u.has_host())
            ret.host = u.host().to_string();
        if (ret.host.has_value() && u.has_authority())
            ret.host->insert(0, "//");
        if (u.has_path())
            ret.path = u.path().to_string();

        if (debug)
        {
            if (u.has_authority())
                ret.auth = u.authority().to_string();
            if (u.has_query())
                ret.query = u.query().to_string();
            if (u.has_fragment())
                ret.fragment = u.fragment().to_string();
        }

        return ret;
    }
    catch (const std::exception&)
    {
        return ret;
    }
}
} // namespace

std::string get_presentable_uri(const std::string& uri, bool debug)
{
    dissected_uri dis_uri = dissect_uri(uri, debug);

    if (debug)
        return to_presentable_internal_debug(dis_uri, uri);
    else
        return to_presentable_internal(dis_uri);
}

} // namespace hlasm_plugin::utils::path