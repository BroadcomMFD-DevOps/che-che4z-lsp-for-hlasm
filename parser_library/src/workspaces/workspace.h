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

#ifndef HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_H
#define HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_H

#include <atomic>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "branch_info.h"
#include "debugging/debugger_configuration.h"
#include "diagnosable_impl.h"
#include "file_manager_vfm.h"
#include "folding_range.h"
#include "lib_config.h"
#include "macro_cache.h"
#include "message_consumer.h"
#include "processor_group.h"
#include "semantics/highlighting_info.h"
#include "utils/resource_location.h"
#include "utils/task.h"
#include "workspace_configuration.h"

namespace hlasm_plugin::parser_library {
struct completion_item;
enum class completion_trigger_kind;
struct document_symbol_item;
struct fade_message;
class external_configuration_requests;
} // namespace hlasm_plugin::parser_library
namespace hlasm_plugin::parser_library::workspaces {
class file_manager;
class library;
class processor_file_impl;
using ws_uri = std::string;
using ws_highlight_info = std::unordered_map<std::string, semantics::highlighting_info>;
struct workspace_parse_lib_provider;
struct parsing_results;
struct parse_file_result
{
    utils::resource::resource_location filename;
    workspace_file_info parse_results;
    std::optional<performance_metrics> metrics_to_report;
    size_t errors = 0;
    size_t warnings = 0;
    bool outputs_changed = false;
};
// Represents a LSP workspace. It solves all dependencies between files -
// implements parse lib provider and decides which files are to be parsed
// when a particular file has been changed in the editor.
class workspace
{
public:
    using resource_location = utils::resource::resource_location;
    using resource_location_hasher = utils::resource::resource_location_hasher;

    workspace(file_manager& file_manager, workspace_configuration& configuration, const lib_config& global_config);

    workspace(const workspace& ws) = delete;
    workspace& operator=(const workspace&) = delete;

    workspace(workspace&& ws) = default;
    workspace& operator=(workspace&&) = delete;

    ~workspace();

    void produce_diagnostics(std::vector<diagnostic>& target) const;
    void include_advisory_configuration_diagnostics(bool include_advisory_cfg_diags);

    [[nodiscard]] utils::task mark_file_for_parsing(
        const resource_location& file_location, file_content_state file_content_status);
    void mark_all_opened_files();
    [[nodiscard]] utils::task did_open_file(
        resource_location file_location, file_content_state file_content_status = file_content_state::changed_content);
    [[nodiscard]] utils::task did_change_file(resource_location file_location, file_content_state file_content_status);
    [[nodiscard]] utils::task did_close_file(resource_location file_location);
    [[nodiscard]] utils::task did_change_watched_files(
        std::vector<resource_location> file_locations, std::vector<file_content_state> file_change_status);

    [[nodiscard]] utils::value_task<parse_file_result> parse_file(
        const resource_location& preferred_file = resource_location());

    location definition(const resource_location& document_loc, position pos) const;
    std::vector<location> references(const resource_location& document_loc, position pos) const;
    std::string hover(const resource_location& document_loc, position pos) const;
    std::vector<completion_item> completion(
        const resource_location& document_loc, position pos, char trigger_char, completion_trigger_kind trigger_kind);
    std::vector<document_symbol_item> document_symbol(const resource_location& document_loc) const;

    std::vector<token_info> semantic_tokens(const resource_location& document_loc) const;

    std::vector<branch_info> branch_information(const resource_location& document_loc) const;

    std::vector<folding_range> folding(const resource_location& document_loc) const;

    std::vector<output_line> retrieve_output(const resource_location& document_loc) const;

    std::optional<performance_metrics> last_metrics(const resource_location& document_loc) const;

    void set_message_consumer(message_consumer* consumer);

    const processor_group* get_proc_grp(const resource_location& file) const;

    std::vector<std::pair<std::string, size_t>> make_opcode_suggestion(
        const resource_location& file, std::string_view opcode, bool extended);

    void retrieve_fade_messages(std::vector<fade_message>& fms) const;

    void external_configuration_invalidated(const resource_location& url);

private:
    file_manager& file_manager_;
    file_manager_vfm fm_vfm_;

    bool is_dependency(const resource_location& file_location) const;

    void show_message(std::string_view message);

    message_consumer* message_consumer_ = nullptr;

    const lib_config& global_config_;

    lib_config get_config() const;

    workspace_configuration& m_configuration;

    bool m_include_advisory_cfg_diags;

    struct dependency_cache
    {
        dependency_cache(version_t version, const file_manager& fm, std::shared_ptr<file> file)
            : version(version)
            , cache(fm, std::move(file))
        {}
        version_t version;
        macro_cache cache;
    };

    struct processor_file_compoments
    {
        std::shared_ptr<file> m_file;
        std::unique_ptr<parsing_results> m_last_results;

        std::map<resource_location, std::variant<std::shared_ptr<dependency_cache>, virtual_file_handle>, std::less<>>
            m_dependencies;
        std::map<std::string, resource_location, std::less<>> m_member_map;

        resource_location m_alternative_config = resource_location();

        bool m_opened = false;
        bool m_collect_perf_metrics = false;

        std::shared_ptr<context::id_storage> m_last_opencode_id_storage;
        bool m_last_opencode_analyzer_with_lsp = false;
        bool m_last_macro_analyzer_with_lsp = false;

        explicit processor_file_compoments(std::shared_ptr<file> file);
        processor_file_compoments(const processor_file_compoments&) = delete;
        processor_file_compoments(processor_file_compoments&&) noexcept;
        processor_file_compoments& operator=(const processor_file_compoments&) = delete;
        processor_file_compoments& operator=(processor_file_compoments&&) noexcept;
        ~processor_file_compoments();

        [[nodiscard]] utils::task update_source_if_needed(file_manager& fm);
    };

    std::unordered_map<resource_location, processor_file_compoments, resource_location_hasher> m_processor_files;
    std::unordered_set<resource_location, resource_location_hasher> m_parsing_pending;

    configuration_diagnostics_parameters get_configuration_diagnostics_params() const;

    [[nodiscard]] utils::value_task<processor_file_compoments&> add_processor_file_impl(std::shared_ptr<file> f);
    const processor_file_compoments* find_processor_file_impl(const resource_location& file) const;
    friend struct workspace_parse_lib_provider;
    workspace_file_info parse_successful(processor_file_compoments& comp, workspace_parse_lib_provider libs);
    void delete_diags(processor_file_compoments& pfc);

    std::vector<const processor_file_compoments*> find_related_opencodes(const resource_location& document_loc) const;
    void filter_and_close_dependencies(std::set<resource_location> files_to_close_candidates,
        const processor_file_compoments* file_to_ignore = nullptr);
};

} // namespace hlasm_plugin::parser_library::workspaces

#endif // !HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_H
