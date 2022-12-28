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

#include "utils/string_operations.h"

namespace hlasm_plugin::utils {

size_t consume(std::string_view& s, std::string_view lit)
{
    // case sensitive
    if (s.substr(0, lit.size()) != lit)
        return 0;
    s.remove_prefix(lit.size());
    return lit.size();
}

std::string_view next_nonblank_sequence(std::string_view s)
{
    if (s.empty() || s.front() == ' ')
        return {};

    auto space = s.find(' ');
    if (space == std::string_view::npos)
        space = s.size();

    return s.substr(0, space);
}

} // namespace hlasm_plugin::utils
