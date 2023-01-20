/*
 * Copyright (c) 2021 Broadcom.
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

#include "logical_line.h"

#include "utils/unicode_text.h"

namespace hlasm_plugin::parser_library::lexing {
std::pair<std::string_view, logical_line_segment_eol> extract_line(std::string_view& input)
{
    auto eol = input.find_first_of("\r\n");
    if (eol == std::string_view::npos)
    {
        std::string_view ret = input;
        input = std::string_view();
        return std::make_pair(ret, logical_line_segment_eol::none);
    }
    else
    {
        auto ret = std::make_pair(input.substr(0, eol), logical_line_segment_eol::lf);
        size_t remove = eol + 1;
        if (input.at(eol) == '\r')
        {
            if (input.size() > eol + 1 && input.at(eol + 1) == '\n')
            {
                ++remove;
                ret.second = logical_line_segment_eol::crlf;
            }
            else
                ret.second = logical_line_segment_eol::cr;
        }
        input.remove_prefix(remove);

        return ret;
    }
}
} // namespace hlasm_plugin::parser_library::lexing
