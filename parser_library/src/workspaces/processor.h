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

#ifndef HLASMPLUGIN_PARSERLIBRARY_PROCESSOR_H
#define HLASMPLUGIN_PARSERLIBRARY_PROCESSOR_H

#include <set>
#include <unordered_map>
#include <vector>

#include "diagnosable.h"
#include "preprocessor_options.h"
#include "protocol.h"
#include "semantics/highlighting_info.h"

namespace hlasm_plugin::utils::resource {
class resource_location;
}

namespace hlasm_plugin::parser_library {
struct analyzing_context;
struct asm_option;
struct fade_message_s;
class virtual_file_monitor;

namespace lsp {
class lsp_context;
}

namespace processing {
struct hit_count_entry;
} // namespace processing

} // namespace hlasm_plugin::parser_library

namespace hlasm_plugin::parser_library::workspaces {
struct library_data;
class parse_lib_provider;

// Interface that represents a file that can be parsed.
class processor_file : public virtual diagnosable
{
public:
    virtual const semantics::lines_info& get_hl_info() = 0;
    virtual const lsp::lsp_context* get_lsp_context() const = 0;
    virtual const performance_metrics& get_metrics() = 0;
    virtual bool has_opencode_lsp_info() const = 0;
    virtual bool has_macro_lsp_info() const = 0;
    virtual const std::vector<fade_message_s>& fade_messages() const = 0;
    virtual const std::unordered_map<utils::resource::resource_location,
        processing::hit_count_entry,
        utils::resource::resource_location_hasher>&
    hit_count_opencode_map() const = 0;
    virtual const std::unordered_map<utils::resource::resource_location,
        processing::hit_count_entry,
        utils::resource::resource_location_hasher>&
    hit_count_macro_map() const = 0;
    virtual const utils::resource::resource_location& get_location() const = 0;
    virtual bool current_version() const = 0;

protected:
    ~processor_file() = default;
};

} // namespace hlasm_plugin::parser_library::workspaces
#endif
