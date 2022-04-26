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

#ifndef HLASMPLUGIN_UTILS_UTF8TEXT_H
#define HLASMPLUGIN_UTILS_UTF8TEXT_H

#include <array>
#include <limits>
#include <string>
#include <string_view>

namespace hlasm_plugin::utils {

// Length of Unicode character in 8/16-bit chunks
struct char_size
{
    uint8_t utf8 : 4;
    uint8_t utf16 : 4;
};

// Map first byte of UTF-8 encoded Unicode character to char_size
constexpr const auto utf8_prefix_sizes = []() {
    std::array<char_size, 256> sizes = {};
    static_assert(std::numeric_limits<unsigned char>::max() < sizes.size());
    for (int i = 0b0000'0000; i <= 0b0111'1111; ++i)
        sizes[i] = { 1, 1 };
    for (int i = 0b1100'0000; i <= 0b1101'1111; ++i)
        sizes[i] = { 2, 1 };
    for (int i = 0b1110'0000; i <= 0b1110'1111; ++i)
        sizes[i] = { 3, 1 };
    for (int i = 0b1111'0000; i <= 0b1111'0111; ++i)
        sizes[i] = { 4, 2 };
    return sizes;
}();

constexpr const char substitute_character = 0x1a;

void append_utf8_sanitized(std::string& result, std::string_view str);
} // namespace hlasm_plugin::utils

#endif