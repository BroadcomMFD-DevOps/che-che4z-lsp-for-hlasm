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

#ifndef HLASMPLUGIN_UTILS_STRING_OPERATIONS_H
#define HLASMPLUGIN_UTILS_STRING_OPERATIONS_H

#include <cctype>
#include <string_view>
#include <utility>

namespace hlasm_plugin::utils {

template<typename S>
size_t trim_left(S& s)
{
    if (s.empty() || s.front() != ' ')
        return 0;

    const auto to_trim = s.find_first_not_of(' ');
    if (to_trim == S::npos)
    {
        auto s_length = s.length();
        s = {};
        return s_length;
    }

    s.remove_prefix(to_trim);
    return to_trim;
}

template<typename S>
size_t trim_right(S& s)
{
    if (s.empty() || s.back() != ' ')
        return 0;

    const auto to_trim = s.find_last_not_of(' ');
    if (to_trim == S::npos)
    {
        auto s_length = s.length();
        s = {};
        return s_length;
    }

    s = s.substr(0, to_trim + 1);
    return to_trim;
}

size_t consume(std::string_view& s, std::string_view lit);
std::string_view next_nonblank_sequence(std::string_view s);

inline bool isblank32(char32_t c) { return c <= 255 && std::isblank(static_cast<unsigned char>(c)); }

} // namespace hlasm_plugin::utils

#endif