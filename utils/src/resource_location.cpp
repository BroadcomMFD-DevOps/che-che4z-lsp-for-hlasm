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

#include <algorithm>
#include <cstddef>
#include <deque>
#include <iterator>

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
    if (!uri.empty())
    {
        if (uri.back() == '\\')
            uri.back() = '/';
        else if (uri.back() != '/')
            uri.append("/");

        if (r.starts_with("/"))
            r.remove_prefix(1);
    }

    uri.append(r);
}

struct uri_path_iterator
{
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::string_view;
    using pointer = std::string_view*;
    using reference = std::string_view;

    uri_path_iterator(pointer uri_path)
        : m_uri_path(uri_path)
        , m_element("")
        , m_last(uri_path == nullptr || uri_path->empty())
        , m_started(m_last)
    {}

    reference operator*() const { return m_element; }
    pointer operator->() { return &m_element; }

    uri_path_iterator& operator++()
    {
        next_element();
        return *this;
    }

    uri_path_iterator operator++(int)
    {
        uri_path_iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    friend auto operator<=>(const uri_path_iterator& l, const uri_path_iterator& r) noexcept = default;

    uri_path_iterator begin()
    {
        next_element();
        return *this;
    }

    uri_path_iterator end() { return uri_path_iterator(nullptr); }

private:
    pointer m_uri_path;
    value_type m_element;
    bool m_last;
    bool m_started;

    void next_element()
    {
        if (m_uri_path == nullptr || m_last == true)
        {
            m_element = "";
            m_uri_path = nullptr;
            m_last = true;
            m_started = true;
            return;
        }

        if (!m_started && !m_uri_path->empty() && (m_uri_path->front() == '/' || m_uri_path->front() == '\\'))
        {
            // First char of the path is a slash - promote it to element
            m_element = m_uri_path->substr(0, 1);
            m_uri_path->remove_prefix(1);
            m_started = true;
            return;
        }
        m_started = true;

        if (auto not_slash = m_uri_path->find_first_not_of("/\\"); not_slash != std::string_view::npos)
        {
            // Store the current element name without any potential ending slash
            auto next_slash = m_uri_path->find_first_of("/\\", not_slash);
            m_element = m_uri_path->substr(not_slash, next_slash - not_slash);

            if (next_slash == std::string_view::npos)
            {
                m_uri_path = nullptr;
                return;
            }

            m_uri_path->remove_prefix(next_slash);
        }
        else
        {
            // If we got here it must mean that the path now consists only of slashes (e.g. '/')
            // The last element is therefore empty but valid
            m_last = true;
            m_element = "";
        }
    };
};

std::string normalize_path(std::string_view path)
{
    std::deque<std::string_view> elements;
    uri_path_iterator this_it(&path);
    for (auto element : this_it)
    {
        if (element == ".")
            continue;
        else if (element == "..")
        {
            if (!elements.empty())
                elements.pop_back();
        }
        else if (elements.empty() && element.empty())
            elements.push_back("/");
        else
            elements.push_back(element);
    }

    std::string ret = "";
    // Check if there is any element to process further
    if (elements.empty())
        return ret;

    // Append elements to uri
    while (!elements.empty())
    {
        uri_append(ret, elements.front());
        elements.pop_front();
    }

    // Add a missing '/' if the last part of the original uri is "/." , "/..", "." or ".."
    if (path == "/." || path == "/.." || path == "." || path == "..")
        uri_append(ret, "");

    return ret;
}
} // namespace

std::string resource_location::lexically_normal()
{
    std::replace(m_uri.begin(), m_uri.end(), '\\', '/');

    auto dis_uri = utils::path::dissect_uri(m_uri);
    if (dis_uri.path.empty())
        return m_uri;

    dis_uri.path = normalize_path(dis_uri.path);

    return utils::path::reconstruct_uri(dis_uri);
}

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

    // Now create iterators and mismatch them
    uri_path_iterator l_it(&this_uri);
    uri_path_iterator r_it(&base_uri);

    auto [this_it, base_it] = std::mismatch(l_it.begin(), l_it.end(), r_it.begin(), r_it.end());
    if (this_it == this_it.end() && base_it == base_it.end())
        return ".";

    // Figure out how many dirs to return
    int16_t number_of_dirs_to_return = 0;
    while (base_it != base_it.end())
    {
        auto element = *base_it;

        if (element.empty()) {}
        else if (element == ".")
        {}
        else if (element == "..")
            number_of_dirs_to_return--;
        else
            number_of_dirs_to_return++;

        base_it++;
    }

    if (number_of_dirs_to_return < 0)
        return "";

    if (number_of_dirs_to_return == 0 && (this_it == this_it.end() || *this_it == ""))
        return ".";

    // Append number of dirs to return
    std::string ret;
    while (number_of_dirs_to_return--)
    {
        uri_append(ret, "..");
    }

    // Append the rest of the remaining path
    while (this_it != this_it.end())
    {
        auto element = *this_it;
        uri_append(ret, element);
        this_it++;
    }

    return ret;
}

bool resource_location::lexically_out_of_scope() const
{
    return m_uri == std::string_view("..") || m_uri.starts_with("../") || m_uri.starts_with("..\\");
}

void resource_location::join(const std::string& other)
{
    if (utils::path::is_uri(other))
        m_uri = other;
    else
        uri_append(m_uri, other);
}

resource_location resource_location::join(resource_location rl, const std::string& other)
{
    rl.join(other);

    return rl;
}

namespace {
utils::path::dissected_uri::authority relative_reference_process_new_auth(
    const std::optional<utils::path::dissected_uri::authority>& old_auth, std::string_view host)
{
    utils::path::dissected_uri::authority new_auth;
    new_auth.host = host;

    if (!old_auth)
    {
        new_auth.user_info = old_auth->user_info;
        new_auth.port = old_auth->port;
    }

    return new_auth;
}

// Merge based on RFC 3986
void merge(std::string& uri, std::string_view r)
{
    if (auto f = uri.find_last_of("/:"); f != std::string::npos)
    {
        uri = uri.substr(0, f + 1);
        uri.append(r);
    }
    else
        uri = r;
}

// Algorithm from RFC 3986
std::string remove_dot_segments(std::string_view path)
{
    std::string ret;
    std::deque<std::string_view> elements;

    while (!path.empty())
    {
        if (path.starts_with("../"))
            path = path.substr(3);
        else if (path.starts_with("./"))
            path = path.substr(2);
        else if (path.starts_with("/./"))
            path = path.substr(2);
        else if (path == "." || path == ".." || path == "/.")
        {
            break;
        }
        else if (path.starts_with("/../"))
        {
            path = path.substr(3);

            if (!elements.empty())
                elements.pop_back();
        }
        else if (path == "/..")
        {
            if (!elements.empty())
                elements.pop_back();
            break;
        }
        else
        {
            auto slash = path.find_first_of("/", 1);
            elements.push_back(path.substr(0, slash));

            if (slash != std::string::npos)
                path = path.substr(slash);
            else
                path.remove_prefix(path.size());
        }
    }

    for (auto& element : elements)
    {
        ret.append(element);
    }

    // Add a missing '/' if the last part of the original uri is "/." or "/.."
    if (path == "/." || path == "/..")
        uri_append(ret, "/");

    return ret;
}
} // namespace

void resource_location::relative_reference_resolution(
    const std::string& other) // TODO enhancements can be made based on rfc3986 if needed
{
    if (other.empty())
        return;

    if (utils::path::is_uri(other))
    {
        auto dis_uri = utils::path::dissect_uri(other);
        dis_uri.path = remove_dot_segments(dis_uri.path);
        m_uri = utils::path::reconstruct_uri(dis_uri);
    }
    else
    {
        auto dis_uri = utils::path::dissect_uri(m_uri);

        if (dis_uri.scheme.empty() && dis_uri.path.empty())
            uri_append(m_uri, other); // This is a regular path and not a uri. Let's proceed in a regular way
        else if (other.front() == '?')
            dis_uri.query = other.substr(1);
        else if (other.front() == '#')
            dis_uri.fragment = other.substr(1);
        else if (other.starts_with("//"))
        {
            dis_uri.auth = relative_reference_process_new_auth(dis_uri.auth, other.substr(2));
            dis_uri.path.clear();
            dis_uri.query = std::nullopt;
            dis_uri.fragment = std::nullopt;
        }
        else
        {
            if (other.starts_with("/"))
                dis_uri.path = other;
            else
                merge(dis_uri.path, other);

            dis_uri.path = remove_dot_segments(dis_uri.path);

            dis_uri.query = std::nullopt;
            dis_uri.fragment = std::nullopt;
        }

        m_uri = utils::path::reconstruct_uri(dis_uri);
    }
}

resource_location resource_location::relative_reference_resolution(resource_location rl, const std::string& other)
{
    rl.relative_reference_resolution(other);

    return rl;
}

std::size_t resource_location_hasher::operator()(const resource_location& rl) const
{
    return std::hash<std::string> {}(rl.get_uri());
}
} // namespace hlasm_plugin::utils::resource