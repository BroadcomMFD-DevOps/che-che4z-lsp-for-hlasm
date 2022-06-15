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

namespace {
void uri_append(std::string& uri, std::string_view r)
{
    if (!uri.empty() && uri.back() != '/')
        uri.append("/");

    uri.append(r);
}

std::string_view move_to_next_element(std::string_view& p)
{
    while (!p.empty() && p.front() == '/')
        p.remove_prefix(1);

    auto next_slash = p.find_first_of('/');
    auto ret = p.substr(0, next_slash);
    p.remove_prefix(ret.size());

    return ret;
}

std::string_view move_to_next_element2(std::string_view& p)
{
    auto not_slash = p.find_first_not_of('/');

    if (not_slash == std::string_view::npos)
        return p;

    p.remove_prefix(not_slash);
    auto next_slash = p.find_first_of("/");
    return p.substr(0, next_slash);
}

void mismatch_uris(std::string_view& l, std::string_view& r)
{
    while (!l.empty() && !r.empty())
    {
        auto el_l = move_to_next_element2(l);
        auto el_r = move_to_next_element2(r);

        if (el_l != el_r)
            break;

        l.remove_prefix(el_l.size());
        r.remove_prefix(el_r.size());
    }
}
} // namespace

std::string resource_location::lexically_relative(const resource_location& base) const
{
    std::string_view this_uri = m_uri;
    std::string_view base_uri = base.get_uri();

    // Compare schemes
    if (auto this_colon = this_uri.find_first_of(":") + 1; this_colon != std::string_view::npos)
    {
        if (0 != this_uri.compare(0, this_colon, base_uri, 0, this_colon))
            return "";

        this_uri.remove_prefix(this_colon);
        base_uri.remove_prefix(this_colon);
    }

    mismatch_uris(this_uri, base_uri);
    if (this_uri.empty() && base_uri.empty())
        return ".";

    int number_of_dirs_to_return = 0;
    while (!base_uri.empty())
    {
        auto element = move_to_next_element(base_uri);

        if (element.empty()) {}
        else if (element == ".")
        {}
        else if (element == "..")
            number_of_dirs_to_return--;
        else
            number_of_dirs_to_return++;
    }

    if (number_of_dirs_to_return < 0)
        return "";

    if (number_of_dirs_to_return == 0 && this_uri.empty()) // TODO: this_iter empty?   this_iter == this_uri.end()?
        return ".";

    std::string ret;
    while (number_of_dirs_to_return--)
    {
        uri_append(ret, "..");
    }

    while (!this_uri.empty())
    {
        auto element = move_to_next_element(this_uri);
        uri_append(ret, element);
    }

    return ret;
}

bool resource_location::lexically_out_of_scope() const
{
    return m_uri == std::string_view("..") || m_uri.starts_with("../") || m_uri.starts_with("..\\");
}

void resource_location::to_directory()
{
    if (!m_uri.empty())
        if (m_uri.back() == '\\')
            m_uri.back() = '/';
        else if (m_uri.back() != '/')
            m_uri.append("/");
}

void resource_location::join(const std::string& other)
{
    if (utils::path::is_uri(other))
        m_uri = other;
    else
    {
        to_directory();
        m_uri.append(other);
    }
}

resource_location resource_location::join(resource_location rl, const std::string& other)
{
    rl.join(other);

    return rl;
}

std::size_t resource_location_hasher::operator()(const resource_location& rl) const
{
    return std::hash<std::string> {}(rl.get_uri());
}
} // namespace hlasm_plugin::utils::resource