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

#ifndef HLASMPLUGIN_PARSERLIBRARY_DEBUGGING_VARIABLE_H
#define HLASMPLUGIN_PARSERLIBRARY_DEBUGGING_VARIABLE_H

#include <functional>

#include "protocol.h"

namespace hlasm_plugin::parser_library::debugging {
//
// Interface that represents a variable to be shown to the user
// through DAP.
struct variable
{
    std::string name;
    std::string value;
    set_type type = set_type::UNDEF_TYPE;
    var_reference_t var_reference = 0;

    std::function<std::vector<variable>()> values;

    bool is_scalar() const noexcept { return !values; }
};

} // namespace hlasm_plugin::parser_library::debugging

#endif // !HLASMPLUGIN_PARSERLIBRARY_DEBUGGING_VARIABLE_H
