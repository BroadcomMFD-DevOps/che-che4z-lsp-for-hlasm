/*
 * Copyright (c) 2026 Broadcom.
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

#ifndef HLASMPLUGIN_PARSERLIBRARY_LEXING_TEXT_RANGE_H
#define HLASMPLUGIN_PARSERLIBRARY_LEXING_TEXT_RANGE_H

#include <iterator>

#include "range.h"

namespace hlasm_plugin::parser_library::lexing {

template<typename It>
range text_range(const It& b, const It& e, size_t lineno_offset)
{
    assert(std::ranges::distance(b, e) >= 0);

    const auto [bx, by] = b.get_coordinates();
    position b_pos(by + lineno_offset, bx);
    if (b == e) // empty range
        return range(b_pos);
    else
    {
        const auto [ex, ey] = std::prev(e).get_coordinates();
        return range(b_pos, position(ey + lineno_offset, ex + 1));
    }
}

} // namespace hlasm_plugin::parser_library::lexing
#endif
