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

#ifndef HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_CONFIGURATION_H
#define HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_CONFIGURATION_H

#include <atomic>
#include <compare>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <version>

#include "config/b4g_config.h"
#include "config/pgm_conf.h"
#include "config/proc_grps.h"
#include "diagnosable.h"
#include "diagnostic.h"
#include "lib_config.h"
#include "processor_group.h"
#include "utils/general_hashers.h"
#include "utils/resource_location.h"
#include "utils/task.h"

namespace hlasm_plugin::parser_library {
class external_configuration_requests;
} // namespace hlasm_plugin::parser_library
namespace hlasm_plugin::parser_library::workspaces {
using program_id = utils::resource::resource_location;
using global_settings_map =
    std::unordered_map<std::string, std::optional<std::string>, utils::hashers::string_hasher, std::equal_to<>>;

struct basic_conf
{
    std::string name;

    auto operator<=>(const basic_conf&) const = default;

    size_t hash() const noexcept { return std::hash<std::string_view>()(name); }
};

struct b4g_conf
{
    std::string name;
    utils::resource::resource_location bridge_json_uri;

    auto operator<=>(const b4g_conf&) const = default;

    size_t hash() const noexcept
    {
        return std::hash<std::string_view>()(name) ^ utils::resource::resource_location_hasher()(bridge_json_uri);
    }
};

struct external_conf
{
    std::shared_ptr<std::string> definition;

    bool operator==(const external_conf& o) const { return *definition == *o.definition; }
    auto operator<=>(const external_conf& o) const { return definition->compare(*o.definition) <=> 0; } // clang 14

    bool operator==(std::string_view o) const { return *definition == o; }
    auto operator<=>(std::string_view o) const { return definition->compare(o) <=> 0; } // clang 14

    size_t hash() const noexcept { return std::hash<std::string_view>()(*definition); }
};

using proc_grp_id = std::variant<basic_conf, b4g_conf, external_conf>;
class file_manager;
struct library_local_options;
// represents pair program => processor group - saves
// information that a program uses certain processor group
struct program
{
    program(program_id prog_id, std::optional<proc_grp_id> pgroup, config::assembler_options asm_opts, bool external)
        : prog_id(std::move(prog_id))
        , pgroup(std::move(pgroup))
        , asm_opts(std::move(asm_opts))
        , external(external)
    {}

    program_id prog_id;
    std::optional<proc_grp_id> pgroup;
    config::assembler_options asm_opts;
    bool external;
};

struct configuration_diagnostics_parameters
{
    std::unordered_map<utils::resource::resource_location,
        std::vector<utils::resource::resource_location>,
        utils::resource::resource_location_hasher>
        used_configs_opened_files_map;

    bool include_advisory_cfg_diags;
};

enum class parse_config_file_result
{
    parsed,
    not_found,
    error,
};

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


class library_options
{
    struct impl
    {
        void (*const deleter)(const void* p) noexcept;
        bool (*const comparer_lt)(const void* l, const void* r) noexcept;
    };
    template<typename T>
    struct impl_t
    {
        static void deleter(const void* p) noexcept { delete static_cast<const T*>(p); }
        static bool comparer_lt(const void* l, const void* r) noexcept
        {
            return *static_cast<const T*>(l) < *static_cast<const T*>(r);
        }
    };
    template<typename T>
    static const impl* get_impl(const T&)
    {
        static
#ifdef _MSC_VER
            // This prevents COMDAT folding
            constinit
#else
            constexpr
#endif //  _MSC_VER
            impl i { &impl_t<T>::deleter, &impl_t<T>::comparer_lt };
        return &i;
    }
    const impl* m_impl;
    const void* m_data;

public:
    template<typename T>
    explicit library_options(T value)
        : m_impl(get_impl(value))
        , m_data(new T(std::move(value)))
    {}
    library_options(library_options&& o) noexcept
        : m_impl(std::exchange(o.m_impl, nullptr))
        , m_data(std::exchange(o.m_data, nullptr))
    {}
    library_options& operator=(library_options&& o) noexcept
    {
        library_options tmp(std::move(o));
        swap(tmp);
        return *this;
    }
    void swap(library_options& o) noexcept
    {
        std::swap(m_impl, o.m_impl);
        std::swap(m_data, o.m_data);
    }
    ~library_options()
    {
        if (m_impl)
            m_impl->deleter(m_data);
    }

    friend bool operator<(const library_options& l, const library_options& r) noexcept
    {
        if (auto c = l.m_impl <=> r.m_impl; c != 0)
            return c < 0;

        return l.m_impl && l.m_impl->comparer_lt(l.m_data, r.m_data);
    }

    template<typename T>
    friend bool operator<(const library_options& l, const T& r) noexcept
    {
        if (auto c = l.m_impl <=> get_impl(r); c != 0)
            return c < 0;
        return impl_t<T>::comparer_lt(l.m_data, &r);
    }
    template<typename T>
    friend bool operator<(const T& l, const library_options& r) noexcept
    {
        if (auto c = get_impl(l) <=> r.m_impl; c != 0)
            return c < 0;
        return impl_t<T>::comparer_lt(&l, r.m_data);
    }
};

class workspace_configuration
{
    static constexpr const char FILENAME_PROC_GRPS[] = "proc_grps.json";
    static constexpr const char FILENAME_PGM_CONF[] = "pgm_conf.json";
    static constexpr const char HLASM_PLUGIN_FOLDER[] = ".hlasmplugin";
    static constexpr const char B4G_CONF_FILE[] = ".bridge.json";
    static constexpr std::string_view NOPROC_GROUP_ID = "*NOPROC*";

    file_manager& m_file_manager;
    utils::resource::resource_location m_location;
    const shared_json& m_global_settings;

    utils::resource::resource_location m_proc_grps_loc;
    utils::resource::resource_location m_pgm_conf_loc;

    template<typename T>
    struct tagged_string_view
    {
        std::string_view value;
    };

    struct proc_grp_id_hasher
    {
        using is_transparent = void;

        size_t operator()(const proc_grp_id& pgid) const noexcept
        {
            return std::visit([](const auto& x) { return x.hash(); }, pgid);
        }

        size_t operator()(const tagged_string_view<external_conf>& external_conf_candidate) const noexcept
        {
            return std::hash<std::string_view>()(external_conf_candidate.value);
        }
    };
    struct proc_grp_id_equal
    {
        using is_transparent = void;

        bool operator()(const proc_grp_id& l, const proc_grp_id& r) const noexcept { return l == r; }
        bool operator()(const proc_grp_id& l, const tagged_string_view<external_conf>& r) const noexcept
        {
            return std::holds_alternative<external_conf>(l) && *std::get<external_conf>(l).definition == r.value;
        }
        bool operator()(const tagged_string_view<external_conf>& l, const proc_grp_id& r) const noexcept
        {
            return std::holds_alternative<external_conf>(r) && l.value == *std::get<external_conf>(r).definition;
        }
    };

    using name_set = std::unordered_set<std::string, utils::hashers::string_hasher, std::equal_to<>>;
    config::proc_grps m_proc_grps_source;
    std::unordered_map<proc_grp_id, processor_group, proc_grp_id_hasher, proc_grp_id_equal> m_proc_grps;
    std::unordered_map<utils::resource::resource_location, name_set, utils::resource::resource_location_hasher>
        m_missing_proc_grps;

    struct tagged_program
    {
        program pgm;
        const void* tag = nullptr;
    };

    struct missing_pgroup_details
    {
        std::string pgroup_name;
        utils::resource::resource_location config_rl;
    };

    using program_related_details = std::variant<tagged_program, missing_pgroup_details>;
    std::map<utils::resource::resource_location, program_related_details> m_exact_pgm_conf;
    std::vector<std::pair<program_related_details, std::regex>> m_regex_pgm_conf;

    struct b4g_config
    {
        std::optional<config::b4g_map> config;
        std::vector<diagnostic_s> diags;
    };

    std::unordered_map<utils::resource::resource_location, b4g_config, utils::resource::resource_location_hasher>
        m_b4g_config_cache;

    global_settings_map m_utilized_settings_values;

    lib_config m_local_config;

    std::vector<diagnostic_s> m_config_diags;

    std::map<std::pair<utils::resource::resource_location, library_options>,
        std::pair<std::shared_ptr<library>, bool>,
        std::less<>>
        m_libraries;

    external_configuration_requests* m_external_configuration_requests;

    std::shared_ptr<library> get_local_library(
        const utils::resource::resource_location& url, const library_local_options& opts);

    void process_processor_group(const config::processor_group& pg,
        std::span<const std::string> fallback_macro_extensions,
        const utils::resource::resource_location& alternative_root,
        std::vector<diagnostic_s>& diags);

    void process_processor_group_library(const config::library& lib,
        const utils::resource::resource_location& alternative_root,
        std::vector<diagnostic_s>& diags,
        std::span<const std::string> fallback_macro_extensions,
        processor_group& prc_grp);
    void process_processor_group_library(const config::dataset& dsn,
        const utils::resource::resource_location& alternative_root,
        std::vector<diagnostic_s>& diags,
        std::span<const std::string> fallback_macro_extensions,
        processor_group& prc_grp);

    void process_processor_group_and_cleanup_libraries(std::span<const config::processor_group> pgs,
        std::span<const std::string> fallback_macro_extensions,
        const utils::resource::resource_location& alternative_root,
        std::vector<diagnostic_s>& diags);

    missing_pgroup_details new_missing_pgroup_helper(name_set& missing_proc_grps,
        std::string missing_pgroup_name,
        utils::resource::resource_location config_rl) const;

    struct pgm_conf_parameters
    {
        proc_grp_id pgroup_id;
        utils::resource::resource_location pgm_rl;
        const utils::resource::resource_location& alternative_cfg_rl;
        const config::assembler_options& asm_opts;
        name_set& missing_proc_grps;
        const void* tag;
    };

    void add_exact_pgm_conf(pgm_conf_parameters params);

    void add_regex_pgm_conf(pgm_conf_parameters params);

    void process_program(
        const config::program_mapping& pgm, name_set& missing_proc_grps, std::vector<diagnostic_s>& diags);

    bool is_config_file(const utils::resource::resource_location& file_location) const;
    bool is_b4g_config_file(const utils::resource::resource_location& file) const;
    std::pair<const program*, bool> get_program_normalized(
        const utils::resource::resource_location& file_location_normalized) const;

    [[nodiscard]] utils::value_task<parse_config_file_result> parse_b4g_config_file(
        const utils::resource::resource_location& cfg_file_rl);

    [[nodiscard]] utils::value_task<parse_config_file_result> load_and_process_config(std::vector<diagnostic_s>& diags);

    [[nodiscard]] utils::value_task<parse_config_file_result> load_proc_config(config::proc_grps& proc_groups,
        global_settings_map& utilized_settings_values,
        std::vector<diagnostic_s>& diags);
    [[nodiscard]] utils::value_task<parse_config_file_result> load_pgm_config(
        config::pgm_conf& pgm_config, global_settings_map& utilized_settings_values, std::vector<diagnostic_s>& diags);

    void find_and_add_libs(const utils::resource::resource_location& root,
        const utils::resource::resource_location& path_pattern,
        processor_group& prc_grp,
        const library_local_options& opts,
        std::vector<diagnostic_s>& diags);

    const missing_pgroup_details* get_missing_pgroup_details(
        const utils::resource::resource_location& file_location) const;

    std::unordered_map<std::string, bool, utils::hashers::string_hasher, std::equal_to<>>
    get_categorized_missing_pgroups(const utils::resource::resource_location& config_file_rl,
        const std::vector<utils::resource::resource_location>& opened_files) const;

    void add_missing_diags(const diagnosable& target,
        const utils::resource::resource_location& config_file_rl,
        const std::vector<utils::resource::resource_location>& opened_files,
        bool include_advisory_cfg_diags) const;

public:
    workspace_configuration(file_manager& fm,
        utils::resource::resource_location location,
        const shared_json& global_settings,
        external_configuration_requests* ecr);

    bool is_configuration_file(const utils::resource::resource_location& file) const;
    [[nodiscard]] utils::value_task<parse_config_file_result> parse_configuration_file(
        std::optional<utils::resource::resource_location> file = std::nullopt);
    [[nodiscard]] utils::value_task<utils::resource::resource_location> load_alternative_config_if_needed(
        const utils::resource::resource_location& file_location);

    const program* get_program(const utils::resource::resource_location& program) const;
    const processor_group* get_proc_grp_by_program(const program& p) const;
    processor_group* get_proc_grp_by_program(const program& p);
    const lib_config& get_config() const { return m_local_config; }

    bool settings_updated() const;
    [[nodiscard]] utils::value_task<std::optional<std::vector<const processor_group*>>> refresh_libraries(
        const std::vector<utils::resource::resource_location>& file_locations);

    void produce_diagnostics(
        const diagnosable& target, const configuration_diagnostics_parameters& config_diag_params) const;

    const processor_group& get_proc_grp(const proc_grp_id& p) const; // test only

    void update_external_configuration(
        const utils::resource::resource_location& normalized_location, std::string group_json);
    decltype(m_proc_grps)::iterator make_external_proc_group(
        const utils::resource::resource_location& normalized_location, std::string group_json);

    void prune_external_processor_groups(const utils::resource::resource_location& location);
};

} // namespace hlasm_plugin::parser_library::workspaces

#endif
