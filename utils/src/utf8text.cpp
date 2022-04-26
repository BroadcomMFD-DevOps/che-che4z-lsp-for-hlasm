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

#include "utils/utf8text.h"

#include <algorithm>

namespace hlasm_plugin::utils {
void append_utf8_sanitized(std::string& result, std::string_view str)
{
    auto it = str.begin();
    auto end = str.end();
    while (true)
    {
        // handle ascii printable characters
        auto first_complex = std::find_if(it, end, [](unsigned char c) { return c < 0x20 || c >= 0x7f; });
        result.append(it, first_complex);
        it = first_complex;
        if (it == end)
            break;


        unsigned char c = *it;
        auto cs = utf8_prefix_sizes[c];
        if (cs.utf8 && (end - it) >= cs.utf8
            && std::all_of(it + 1, it + cs.utf8, [](unsigned char c) { return (c & 0xC0) == 0x80; }))
        {
            char32_t combined = c & ~(0xffu << (8 - cs.utf8));
            for (auto p = it + 1; p != it + cs.utf8; ++p)
                combined = combined << 6 | *p & 0x3fu;

            if (combined >= 0x20 && combined != 0x7f && combined < 0x8d
                || combined > 0x9f && (0xfffe & combined) != 0xfffe && (combined < 0xfdd0 || combined > 0xfdef))
            {
                result.append(it, it + cs.utf8);
                it += cs.utf8;
                continue;
            }
        }

        static constexpr char hex_digits[] = "0123456789ABCDEF";

        // 0x00-0x1F, 0x7F, 0x8D-0x9F, not characters and invalid sequences
        result.append(1, '<');
        result.append(1, hex_digits[(c >> 4) & 0xf]);
        result.append(1, hex_digits[(c >> 0) & 0xf]);
        result.append(1, '>');

        ++it;
    }
}
} // namespace hlasm_plugin::utils
