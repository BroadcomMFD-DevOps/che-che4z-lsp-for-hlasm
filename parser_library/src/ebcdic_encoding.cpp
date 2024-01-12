/*
 * Copyright (c) 2019 Broadcom.
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

#include "ebcdic_encoding.h"

namespace hlasm_plugin::parser_library {

unsigned char ebcdic_encoding::to_ebcdic_multibyte(const char*& c) noexcept
{
    const auto first_byte = (unsigned char)*(c + 0);
    const auto second_byte = (unsigned char)*(c + 1);
    if (second_byte == 0)
    {
        ++c;
        return EBCDIC_SUB;
    }

    if ((first_byte & 0xE0) == 0xC0) // 110xxxxx 10xxxxxx
    {
        const auto value = ((first_byte & 0x1F) << 6) | (second_byte & 0x3F);
        c += 2;
        return value < std::ssize(a2e) ? a2e[value] : EBCDIC_SUB;
    }

    const auto third_byte = (unsigned char)*(c + 2);
    if (third_byte == 0)
    {
        c += 2;
        return EBCDIC_SUB;
    }

    if (first_byte == (0b11100000 | ebcdic_encoding::unicode_private >> 4)
        && (second_byte & 0b11111100) == (0x80 | (ebcdic_encoding::unicode_private & 0xF) << 2)
        && (third_byte & 0xC0) == 0x80) // our private plane
    {
        unsigned char ebcdic_value = (second_byte & 3) << 6 | third_byte & 0x3f;
        c += 3;
        return ebcdic_value;
    }

    if ((first_byte & 0xF0) == 0xE0) // 1110xxxx 10xxxxxx 10xxxxxx
    {
        c += 3;
        return EBCDIC_SUB;
    }

    const auto fourth_byte = (unsigned char)*(c + 2);
    if (fourth_byte == 0)
    {
        c += 3;
        return EBCDIC_SUB;
    }

    if ((first_byte & 0xF8) == 0xF0) // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    {
        c += 4;
        return EBCDIC_SUB;
    }

    ++c;
    return EBCDIC_SUB;
}

std::string ebcdic_encoding::to_ascii(const std::string& s)
{
    std::string a;
    a.reserve(s.length());
    for (unsigned char c : s)
        a.append(to_ascii(c));
    return a;
}

std::string ebcdic_encoding::to_ebcdic(const std::string& s)
{
    std::string a;
    a.reserve(s.length());
    for (const char* i = s.c_str(); *i != 0;)
        a.push_back(to_ebcdic(i));
    return a;
}
} // namespace hlasm_plugin::parser_library
