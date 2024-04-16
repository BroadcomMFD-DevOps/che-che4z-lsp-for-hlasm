/*
 * Copyright (c) 2023 Broadcom.
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

#include "diagnostic_tools.h"

#include <string>
#include <utility>

#include "diagnostic.h"

namespace hlasm_plugin::parser_library {

diagnostic add_stack_details(diagnostic_op d, context::processing_stack_t stack)
{
    if (stack.empty())
        return diagnostic(std::move(d));

    diagnostic diag(stack.frame().resource_loc->get_uri(), std::move(d));

    for (stack = stack.parent(); !stack.empty(); stack = stack.parent())
    {
        const auto& f = stack.frame();
        diag.related.emplace_back(range_uri_s(f.resource_loc->get_uri(), range(f.pos)),
            "While compiling " + f.resource_loc->to_presentable() + '(' + std::to_string(f.pos.line + 1) + ")");
    }

    return diag;
}

} // namespace hlasm_plugin::parser_library
