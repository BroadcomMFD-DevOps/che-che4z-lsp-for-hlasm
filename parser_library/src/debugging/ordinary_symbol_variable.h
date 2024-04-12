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

#ifndef HLASMPLUGIN_PARSERLIBRARY_DEBUGGING_ORDINARY_SYMBOL_VARIABLE_H
#define HLASMPLUGIN_PARSERLIBRARY_DEBUGGING_ORDINARY_SYMBOL_VARIABLE_H

#include "variable.h"

namespace hlasm_plugin::parser_library::context {
class symbol;
} // namespace hlasm_plugin::parser_library::context

namespace hlasm_plugin::parser_library::debugging {

variable generate_ordinary_symbol_variable(const context::symbol& symbol);

} // namespace hlasm_plugin::parser_library::debugging


#endif // !HLASMPLUGIN_PARSERLIBRARY_DEBUGGING_MACRO_PARAM_VARIABLE_H
