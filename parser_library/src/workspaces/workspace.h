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
#include <filesystem>
#include <memory>
#include <optional>
#include <regex>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <version>

#include "config/b4g_config.h"
#include "config/pgm_conf.h"
#include "config/proc_grps.h"
#include "diagnosable_impl.h"
#include "file_manager.h"
#include "file_manager_vfm.h"
#include "lib_config.h"
#include "library.h"
#include "message_consumer.h"
#include "processor.h"
#include "processor_group.h"
#include "utils/general_hashers.h"
#include "utils/resource_location.h"

namespace hlasm_plugin::parser_library::workspaces {

using ws_uri = std::string;
using proc_grp_id = std::pair<std::string, utils::resource::resource_location>;
using program_id = utils::resource::resource_location;
using ws_highlight_info = std::unordered_map<std::string, semantics::highlighting_info>;
struct library_local_options;

// represents pair program => processor group - saves
// information that a program uses certain processor group
struct program
{
    program(program_id prog_id, proc_grp_id pgroup, config::assembler_options asm_opts)
        : prog_id(std::move(prog_id))
        , pgroup(std::move(pgroup))
        , asm_opts(std::move(asm_opts))
    {}

    program_id prog_id;
    proc_grp_id pgroup;
    config::assembler_options asm_opts;
};


// Represents a LSP workspace. It solves all dependencies between files -
// implements parse lib provider and decides which files are to be parsed
// when a particular file has been changed in the editor.
class workspace : public diagnosable_impl, public parse_lib_provider, public lsp::feature_provider
{
public:
#if __cpp_lib_atomic_shared_ptr >= 201711L
    using shared_json = std::atomic<std::shared_ptr<const nlohmann::json>>;
#else
    class shared_json
    {
        std::shared_ptr<const nlohmann::json> m_data;

    public:
        shared_json(std::shared_ptr<const nlohmann::json> data)
            : m_data(std::move(data))
        {}

        auto load() const { return std::atomic_load(&m_data); }
        void store(std::shared_ptr<const nlohmann::json> data) { std::atomic_store(&m_data, std::move(data)); }
    };
#endif

    // Creates just a dummy workspace with no libraries - no dependencies
    // between files.
    workspace(file_manager& file_manager,
        const lib_config& global_config,
        const shared_json& global_settings,
        std::atomic<bool>* cancel = nullptr);
    workspace(const utils::resource::resource_location& location,
        file_manager& file_manager,
        const lib_config& global_config,
        const shared_json& global_settings,
        std::atomic<bool>* cancel = nullptr);
    workspace(const utils::resource::resource_location& location,
        const std::string& name,
        file_manager& file_manager,
        const lib_config& global_config,
        const shared_json& global_settings,
        std::atomic<bool>* cancel = nullptr);

    workspace(const workspace& ws) = delete;
    workspace& operator=(const workspace&) = delete;

    workspace(workspace&& ws) = default;
    workspace& operator=(workspace&&) = delete;

    void collect_diags() const override;

    void add_proc_grp(processor_group pg, const utils::resource::resource_location& alternative_root);
    const processor_group& get_proc_grp(const proc_grp_id& proc_grp) const;
    const processor_group& get_proc_grp_by_program(const utils::resource::resource_location& file_location) const;
    const processor_group& get_proc_grp_by_program(const program& program) const;

    workspace_file_info parse_file(const utils::resource::resource_location& file_location);
    workspace_file_info parse_successful(const processor_file_ptr& f);
    void refresh_libraries();
    workspace_file_info did_open_file(const utils::resource::resource_location& file_location);
    void did_close_file(const utils::resource::resource_location& file_location);
    void did_change_file(
        const utils::resource::resource_location& file_location, const document_change* changes, size_t ch_size);
    void did_change_watched_files(const std::vector<utils::resource::resource_location>& file_locations);

    location definition(const utils::resource::resource_location& document_loc, position pos) const override;
    location_list references(const utils::resource::resource_location& document_loc, position pos) const override;
    lsp::hover_result hover(const utils::resource::resource_location& document_loc, position pos) const override;
    lsp::completion_list_s completion(const utils::resource::resource_location& document_loc,
        position pos,
        char trigger_char,
        completion_trigger_kind trigger_kind) const override;
    lsp::document_symbol_list_s document_symbol(
        const utils::resource::resource_location& document_loc, long long limit) const override;

    parse_result parse_library(const std::string& library, analyzing_context ctx, library_data data) override;
    bool has_library(const std::string& library, const utils::resource::resource_location& program) const override;
    std::optional<std::string> get_library(const std::string& library,
        const utils::resource::resource_location& program,
        std::optional<utils::resource::resource_location>& location) const override;
    virtual asm_option get_asm_options(const utils::resource::resource_location& file_location) const;
    virtual std::vector<preprocessor_options> get_preprocessor_options(
        const utils::resource::resource_location& file_location) const;
    const ws_uri& uri() const;

    void open();
    void close();

    void set_message_consumer(message_consumer* consumer);

    processor_file_ptr get_processor_file(const utils::resource::resource_location& file_location);

    file_manager& get_file_manager();

    bool settings_updated();

    using global_settings_map =
        std::unordered_map<std::string, std::optional<std::string>, utils::hashers::string_hasher, std::equal_to<>>;

private:
    constexpr static char FILENAME_PROC_GRPS[] = "proc_grps.json";
    constexpr static char FILENAME_PGM_CONF[] = "pgm_conf.json";
    constexpr static char HLASM_PLUGIN_FOLDER[] = ".hlasmplugin";
    constexpr static char B4G_CONF_FILE[] = ".bridge.json";

    std::atomic<bool>* cancel_;

    std::string name_;
    utils::resource::resource_location location_;
    file_manager& file_manager_;
    file_manager_vfm fm_vfm_;

    config::proc_grps m_proc_grps_source;

    struct proc_grp_id_hasher
    {
        size_t operator()(const proc_grp_id& pgid) const
        {
            return std::hash<std::string>()(pgid.first) ^ utils::resource::resource_location_hasher()(pgid.second);
        }
    };

    std::unordered_map<proc_grp_id, processor_group, proc_grp_id_hasher> proc_grps_;

    struct tagged_program
    {
        program pgm;
        const void* tag = nullptr;
    };


    std::map<utils::resource::resource_location, tagged_program> exact_pgm_conf_;
    std::vector<std::pair<tagged_program, std::regex>> regex_pgm_conf_;
    global_settings_map m_utilized_settings_values;
    processor_group implicit_proc_grp;

    struct b4g_config
    {
        std::optional<config::b4g_map> config;
        std::vector<diagnostic_s> diags;
    };

    std::unordered_map<utils::resource::resource_location, b4g_config, utils::resource::resource_location_hasher>
        m_b4g_config_cache;

    utils::resource::resource_location proc_grps_loc_;
    utils::resource::resource_location pgm_conf_loc_;

    bool opened_ = false;

    void find_and_add_libs(const utils::resource::resource_location& root,
        const utils::resource::resource_location& path_pattern,
        processor_group& prc_grp,
        const library_local_options& opts);

    void process_processor_group(const config::processor_group& pg,
        std::span<const std::string> fallback_macro_extensions,
        std::span<const std::string> always_recognize,
        const utils::resource::resource_location& alternative_root);

    bool process_program(const config::program_mapping& pgm);

    bool is_config_file(const utils::resource::resource_location& file_location) const;
    void reparse_after_config_refresh();
    enum class parse_config_file_result
    {
        parsed,
        not_found,
        error,
    };
    parse_config_file_result parse_config_file();
    parse_config_file_result parse_b4g_config_file(const utils::resource::resource_location& file_location);

    bool try_loading_alternative_configuration(const utils::resource::resource_location& file_location);

    bool load_and_process_config();

    bool load_proc_config(
        config::proc_grps& proc_groups, file_ptr& proc_grps_file, global_settings_map& utilized_settings_values);
    bool load_pgm_config(
        config::pgm_conf& pgm_config, file_ptr& pgm_conf_file, global_settings_map& utilized_settings_values);

    // files, that depend on others (e.g. open code files that use macros)
    std::set<utils::resource::resource_location> dependants_;

    std::set<utils::resource::resource_location> opened_files_;

    diagnostic_container config_diags_;

    void filter_and_close_dependencies_(
        const std::set<utils::resource::resource_location>& dependencies, processor_file_ptr file);
    bool is_dependency_(const utils::resource::resource_location& file_location);

    std::vector<processor_file_ptr> find_related_opencodes(
        const utils::resource::resource_location& document_loc) const;
    void delete_diags(processor_file_ptr file);

    void show_message(const std::string& message);

    message_consumer* message_consumer_ = nullptr;

    // A map that holds true values for files that have diags suppressed and the user was already notified about it
    std::unordered_map<utils::resource::resource_location, bool, utils::resource::resource_location_hasher>
        diag_suppress_notified_;
    const lib_config& global_config_;
    const shared_json& m_global_settings;
    lib_config local_config_;
    lib_config get_config();

    const program* get_program(const utils::resource::resource_location& program) const;
    const program* get_program_normalized(const utils::resource::resource_location& file_location) const;
    void load_alternative_config_if_needed(const utils::resource::resource_location& file_location);
};

} // namespace hlasm_plugin::parser_library::workspaces

#endif // !HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_H
