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

#include "ca_expr_list_seta.h"

namespace hlasm_plugin::parser_library::expressions {

ca_expr_list_seta::ca_expr_list_seta(std::vector<ca_expr_ptr> expr_list, range expr_range)
    : ca_expr_list(std::move(expr_list), std::move(expr_range))
{}
} // namespace hlasm_plugin::parser_library::expressions
