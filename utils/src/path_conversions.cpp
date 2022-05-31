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

} // namespace hlasm_plugin::utils::path