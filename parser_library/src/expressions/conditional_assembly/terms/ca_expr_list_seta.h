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

#ifndef HLASMPLUGIN_PARSERLIBRARY_CA_EXPR_LIST_SETA_H
#define HLASMPLUGIN_PARSERLIBRARY_CA_EXPR_LIST_SETA_H

#include "ca_expr_list.h"

namespace hlasm_plugin::parser_library::expressions {

// represents unresolved list of terms in arithmetic CA expression
class ca_expr_list_seta final : public ca_expr_list
{
public:
    ca_expr_list_seta(std::vector<ca_expr_ptr> expr_list, range expr_range);

    bool is_compatible(ca_expression_compatibility) const override { return false; }
};

} // namespace hlasm_plugin::parser_library::expressions

#endif
