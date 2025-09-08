/*
 * Copyright (c) 2025 Broadcom.
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

#include "copy_member.h"

namespace hlasm_plugin::parser_library::context {

copy_member::copy_member(id_index name, statement_block definition, location definition_location)
    : processing::statement_cache(std::move(definition))
    , name(name)
    , definition_location(std::move(definition_location))
{}

} // namespace hlasm_plugin::parser_library::context
