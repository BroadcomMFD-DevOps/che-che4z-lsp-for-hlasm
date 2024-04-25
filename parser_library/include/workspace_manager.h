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

#ifndef HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_MANAGER_H
#define HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_MANAGER_H

// This file specifies the interface of parser library.
// The pimpl (pointer to implementation) idiom is used to hide its implementation.
// It implements LSP requests and notifications and is used by the language server.

#include <atomic>
#include <memory>
#include <span>
#include <utility>

#include "branch_info.h"
#include "folding_range.h"
#include "lib_config.h"
#include "message_consumer.h"
#include "protocol.h"
#include "sequence.h"
#include "workspace_manager_requests.h"

namespace hlasm_plugin::parser_library {
struct completion_item;
struct diagnostic;
struct document_symbol_item;
struct fade_message;
class workspace_manager_external_file_requests;
class external_configuration_requests;
namespace debugging {
struct debugger_configuration;
} // namespace debugging

// Interface that can be implemented to be able to get list of
// diagnostics from workspace manager whenever a file is parsed
// Passes list of all diagnostics that are in all currently opened files.
class diagnostics_consumer
{
public:
    virtual void consume_diagnostics(
        std::span<const diagnostic> diagnostics, std::span<const fade_message> fade_messages) = 0;

protected:
    ~diagnostics_consumer() = default;
};

// Interface that can be implemented to be able to get performance metrics
//(time that parsing took, number of parsed lines, etc)
class parsing_metadata_consumer
{
public:
    virtual void consume_parsing_metadata(sequence<char> uri, double duration, const parsing_metadata& metrics) = 0;
    virtual void outputs_changed(sequence<char> uri) = 0;

protected:
    ~parsing_metadata_consumer() = default;
};

enum class fs_change_type
{
    invalid = 0,
    created = 1,
    changed = 2,
    deleted = 3,
};

struct fs_change
{
    sequence<char> uri;
    fs_change_type change_type;
};

struct opcode_suggestion
{
    continuous_sequence<char> opcode;
    size_t distance;
};

template<typename T>
class workspace_manager_response;

class debugger_configuration_provider
{
protected:
    ~debugger_configuration_provider() = default;

public:
    virtual void provide_debugger_configuration(
        sequence<char> document_uri, workspace_manager_response<debugging::debugger_configuration> conf) = 0;
};

class workspace_manager
{
public:
    virtual ~workspace_manager() = default;
    virtual void add_workspace(const char* name, const char* uri) = 0;
    virtual void remove_workspace(const char* uri) = 0;

    virtual void did_open_file(const char* document_uri, version_t version, const char* text, size_t text_size) = 0;
    virtual void did_change_file(
        const char* document_uri, version_t version, const document_change* changes, size_t ch_size) = 0;
    virtual void did_close_file(const char* document_uri) = 0;
    virtual void did_change_watched_files(sequence<fs_change> changes) = 0;

    virtual void definition(const char* document_uri, position pos, workspace_manager_response<position_uri> resp) = 0;
    virtual void references(
        const char* document_uri, position pos, workspace_manager_response<position_uri_list> resp) = 0;
    virtual void hover(const char* document_uri, position pos, workspace_manager_response<sequence<char>> resp) = 0;
    virtual void completion(const char* document_uri,
        position pos,
        char trigger_char,
        completion_trigger_kind trigger_kind,
        workspace_manager_response<std::span<const completion_item>> resp) = 0;

    virtual void semantic_tokens(
        const char* document_uri, workspace_manager_response<continuous_sequence<token_info>> resp) = 0;
    virtual void document_symbol(
        const char* document_uri, workspace_manager_response<std::span<const document_symbol_item>> resp) = 0;

    virtual void configuration_changed(const lib_config& new_config) = 0;

    // implementation of observer pattern - register consumer.
    virtual void register_diagnostics_consumer(diagnostics_consumer* consumer) = 0;
    virtual void unregister_diagnostics_consumer(diagnostics_consumer* consumer) = 0;
    virtual void register_parsing_metadata_consumer(parsing_metadata_consumer* consumer) = 0;
    virtual void unregister_parsing_metadata_consumer(parsing_metadata_consumer* consumer) = 0;
    virtual void set_message_consumer(message_consumer* consumer) = 0;
    virtual void set_request_interface(workspace_manager_requests* requests) = 0;

    virtual continuous_sequence<char> get_virtual_file_content(unsigned long long id) const = 0;

    virtual void toggle_advisory_configuration_diagnostics() = 0;

    virtual void make_opcode_suggestion(const char* document_uri,
        const char* opcode,
        bool extended,
        workspace_manager_response<continuous_sequence<opcode_suggestion>>) = 0;

    virtual void idle_handler(const std::atomic<unsigned char>* yield_indicator = nullptr) = 0;

    virtual debugger_configuration_provider& get_debugger_configuration_provider() = 0;

    virtual void invalidate_external_configuration(sequence<char> uri) = 0;

    virtual void branch_information(
        const char* document_uri, workspace_manager_response<continuous_sequence<branch_info>> resp) = 0;

    virtual void folding(
        const char* document_uri, workspace_manager_response<continuous_sequence<folding_range>> resp) = 0;

    virtual void retrieve_output(
        const char* document_uri, workspace_manager_response<continuous_sequence<output_line>> resp) = 0;
};

workspace_manager* create_workspace_manager_impl(
    workspace_manager_external_file_requests* external_requests, bool vscode_extensions);
inline std::unique_ptr<workspace_manager> create_workspace_manager(
    workspace_manager_external_file_requests* external_requests = nullptr, bool vscode_extensions = false)
{
    return std::unique_ptr<workspace_manager>(create_workspace_manager_impl(external_requests, vscode_extensions));
}

} // namespace hlasm_plugin::parser_library
#endif // !HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_MANAGER_H
