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

#ifndef HLASMPLUGIN_PARSERLIBRARY_PROTOCOL_H
#define HLASMPLUGIN_PARSERLIBRARY_PROTOCOL_H

// This file contains definitions of types that the workspace
// manager uses to pass data in and out of parser library.
// Most of them use pimpl to hide their implementation somewhere
// in the library.
// Most of the types are C++ representation of LSP/DAP data types.

#include <cstdint>
#include <cstring>

#include "range.h"
#include "sequence.h"

#pragma warning(push)
#pragma warning(disable : 4661)

namespace hlasm_plugin::parser_library {

using version_t = uint64_t;

// in case any changes are done to these scopes, the tokenTypes field in feature_language_features.cpp
// needs to be adjusted accordingly, as they are implicitly but directly mapped to each other
enum class hl_scopes : size_t
{
    label = 0,
    instruction = 1,
    remark = 2,
    ignored = 3,
    comment = 4,
    continuation = 5,
    seq_symbol = 6,
    var_symbol = 7,
    operator_symbol = 8,
    string = 9,
    number = 10,
    operand = 11,
    data_def_type = 12,
    data_def_modifier = 13,
    data_attr_type = 14,
    self_def_type = 15,
    ordinary_symbol = 16
};

namespace debugging {

struct stack_frame;
struct source;
struct scope;
struct variable;
struct variable_store;
} // namespace debugging

struct document_change
{
    document_change(const char* new_text, size_t text_length)
        : whole(true)
        , text(new_text)
        , text_length(text_length)
    {}
    document_change(range change_range, const char* new_text, size_t text_length)
        : whole(false)
        , change_range(change_range)
        , text(new_text)
        , text_length(text_length)
    {}

    bool operator==(const document_change& ch) const
    {
        return whole == ch.whole && change_range == ch.change_range && text_length == ch.text_length
            && std::memcmp(text, ch.text, text_length) == 0;
    }

    const bool whole;
    const range change_range;
    const char* const text;
    const size_t text_length;
};

struct text_document_item
{
    char* document_uri;
    version_t version;
    char* text;
};

struct performance_metrics
{
    size_t lines = 0;
    size_t macro_def_statements = 0;
    size_t macro_statements = 0;
    size_t open_code_statements = 0;
    size_t copy_def_statements = 0;
    size_t copy_statements = 0;
    size_t reparsed_statements = 0;
    size_t lookahead_statements = 0;
    size_t continued_statements = 0;
    size_t non_continued_statements = 0;

    bool operator==(const performance_metrics&) const noexcept = default;
};

struct workspace_file_info
{
    size_t files_processed = 0;
    bool config_parsing = false;
    bool diagnostics_suppressed = false;
    bool processor_group_found = false;
};

struct parsing_metadata
{
    performance_metrics metrics;
    workspace_file_info ws_info;
    size_t errors = 0;
    size_t warnings = 0;
};

struct token_info
{
    token_info(const range& token_range, hl_scopes scope)
        : token_range(token_range)
        , scope(scope)
    {}
    token_info(position start, position end, hl_scopes scope)
        : token_range(start, end)
        , scope(scope)
    {}
    token_info(size_t line_start, size_t column_start, size_t line_end, size_t column_end, hl_scopes scope)
        : token_range({ line_start, column_start }, { line_end, column_end })
        , scope(scope)
    {}
    range token_range;
    hl_scopes scope;

    bool operator==(const token_info& rhs) const noexcept = default;
};

struct source
{
    source(const debugging::source& source);

    sequence<char> uri;
};

struct stack_frame
{
    explicit stack_frame(const debugging::stack_frame& frame);

    sequence<char> name;
    source source_file;
    range source_range;
    uint32_t id;
};

using stack_frames_t = sequence<stack_frame, const debugging::stack_frame*>;

using frame_id_t = size_t;
using var_reference_t = size_t;

enum class set_type
{
    A_TYPE,
    B_TYPE,
    C_TYPE,
    UNDEF_TYPE
};

struct scope
{
    explicit scope(const debugging::scope& impl);

    sequence<char> name;
    var_reference_t variable_reference;
    source source_file;
};

using scopes_t = sequence<scope, const debugging::scope*>;

struct variable
{
    explicit variable(const debugging::variable& impl);

    sequence<char> name;
    sequence<char> value;
    var_reference_t variable_reference;
    set_type type;
};

using variables_t = sequence<variable, const debugging::variable_store*>;

struct breakpoint
{
    explicit breakpoint(size_t line)
        : line(line)
    {}
    size_t line;
};

struct function_breakpoint
{
    explicit function_breakpoint(sequence<char> name)
        : name(name)
    {}
    sequence<char> name;
};

struct output_line
{
    int level; // -1 if N/A
    continuous_sequence<char> text;
};

} // namespace hlasm_plugin::parser_library
#pragma warning(pop)
#endif // !HLASMPLUGIN_PARSERLIBRARY_PROTOCOL_H
