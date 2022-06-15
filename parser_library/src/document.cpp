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

#include "document.h"

namespace hlasm_plugin::parser_library {
document::document(std::string_view text)
{
    size_t line_no = 0;
    while (!text.empty())
    {
        auto p = text.find_first_of("\r\n");
        if (p == std::string_view::npos)
            break;
        if (text.substr(p, 2) == "\r\n")
            ++p;

        m_lines.emplace_back(original_line { text.substr(0, p + 1), line_no });

        text.remove_prefix(p + 1);
        ++line_no;
    }
    if (!text.empty())
        m_lines.emplace_back(original_line { text, line_no });
}
} // namespace hlasm_plugin::parser_library
