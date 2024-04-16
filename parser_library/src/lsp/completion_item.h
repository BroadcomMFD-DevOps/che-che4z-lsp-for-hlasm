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

#ifndef LSP_COMPLETION_ITEM_H
#define LSP_COMPLETION_ITEM_H

#include <string>
#include <vector>

#include "protocol.h"

namespace hlasm_plugin::parser_library::lsp {

// representation of completion item based on LSP
struct completion_item_s
{
    // contents directly passed via the constructor
    completion_item_s(std::string label,
        std::string detail,
        std::string insert_text,
        std::string documentation,
        completion_item_kind kind = completion_item_kind::mach_instr,
        bool snippet = false,
        std::string suggestion_for = {});

    // several features of completion item from LSP
    std::string label;
    std::string detail;
    std::string insert_text;
    std::string documentation;
    completion_item_kind kind;
    bool snippet = false;
    std::string suggestion_for;

    bool operator==(const completion_item_s&) const = default;
};

} // namespace hlasm_plugin::parser_library::lsp

#endif
