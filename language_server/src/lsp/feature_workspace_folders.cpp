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

#include "feature_workspace_folders.h"

#include "../logger.h"
#include "lib_config.h"
#include "nlohmann/json.hpp"
#include "utils/path.h"
#include "utils/path_conversions.h"
#include "utils/scope_exit.h"

namespace hlasm_plugin::language_server::lsp {

struct feature_workspace_folders::watcher_registration
{
    std::string base_uri;
    bool recursive;
    watcher_id id;
    std::weak_ptr<const void> registration;
};

feature_workspace_folders::feature_workspace_folders(
    parser_library::workspace_manager& ws_mngr, response_provider& response_provider)
    : feature(response_provider)
    , ws_mngr_(ws_mngr)
{}
feature_workspace_folders::~feature_workspace_folders() = default;

void feature_workspace_folders::register_methods(std::map<std::string, method>& methods)
{
    methods.try_emplace("workspace/didChangeWorkspaceFolders",
        method { [this](const nlohmann::json& args) { on_did_change_workspace_folders(args); },
            telemetry_log_level::LOG_EVENT });
    methods.try_emplace("workspace/didChangeWatchedFiles",
        method { [this](const nlohmann::json& args) { did_change_watched_files(args); },
            telemetry_log_level::NO_TELEMETRY });
    methods.try_emplace("workspace/didChangeConfiguration",
        method { [this](const nlohmann::json& args) { did_change_configuration(args); },
            telemetry_log_level::NO_TELEMETRY });
}

nlohmann::json feature_workspace_folders::register_capabilities()
{
    return nlohmann::json { {
        "workspace",
        {
            {
                "workspaceFolders",
                {
                    { "supported", true },
                    { "changeNotifications", true },
                },
            },
        },
    } };
}

void feature_workspace_folders::initialize_feature(const nlohmann::json& initialize_params)
{
    bool ws_folders_support = false;
    const auto& capabs = initialize_params.at("capabilities");

    if (auto ws = capabs.find("workspace"); ws != capabs.end())
    {
        if (auto ws_folders = ws->find("workspaceFolders"); ws_folders != ws->end())
            ws_folders_support = ws_folders->get<bool>();

        if (auto watcher = ws->find("didChangeWatchedFiles"); watcher != ws->end() && watcher->is_object())
        {
            m_supports_dynamic_file_change_notification = watcher->value("dynamicRegistration", false);
            m_supports_file_change_notification_relative_pattern = watcher->value("relativePatternSupport", false);
        }
    }

    if (auto root_uri = initialize_params.find("rootUri"); root_uri != initialize_params.end() && root_uri->is_string())
    {
        m_root_uri = root_uri->get<std::string>();
    }
    else if (auto root_path = initialize_params.find("rootPath");
        root_path != initialize_params.end() && root_path->is_string())
    {
        m_root_uri = utils::path::path_to_uri(utils::path::lexically_normal(root_path->get<std::string>()).string());
    }

    if (ws_folders_support)
    {
        for (const auto& ws : initialize_params.at("workspaceFolders"))
            m_initial_workspaces.emplace_back(ws.at("name").get<std::string>(), ws.at("uri").get<std::string>());
    }
    else if (!m_root_uri.empty())
    {
        m_initial_workspaces.emplace_back(m_root_uri, m_root_uri);
    }
}

void feature_workspace_folders::initialized()
{
    ws_mngr_.change_implicit_group_base(m_root_uri);
    send_configuration_request();
    if (m_supports_dynamic_file_change_notification)
        register_file_change_notifictions();
    for (const auto& [name, uri] : std::exchange(m_initial_workspaces, {}))
        ws_mngr_.add_workspace(name, uri);
}

void feature_workspace_folders::on_did_change_workspace_folders(const nlohmann::json& params)
{
    const auto& added = params.at("event").at("added");
    const auto& removed = params.at("event").at("removed");

    remove_workspaces(removed);
    add_workspaces(added);
}

void feature_workspace_folders::add_workspaces(const nlohmann::json& added)
{
    for (const auto& ws : added)
    {
        ws_mngr_.add_workspace(ws.at("name").get<std::string_view>(), ws.at("uri").get<std::string_view>());
    }
}

void feature_workspace_folders::remove_workspaces(const nlohmann::json& removed)
{
    for (const auto& ws : removed)
    {
        ws_mngr_.remove_workspace(ws.at("uri").get<std::string_view>());
    }
}

void feature_workspace_folders::did_change_watched_files(const nlohmann::json& params)
{
    using namespace hlasm_plugin::parser_library;
    try
    {
        const auto& json_changes = params.at("changes");
        std::vector<fs_change> changes;
        changes.reserve(json_changes.size());
        std::ranges::transform(json_changes, std::back_inserter(changes), [](const nlohmann::json& change) {
            auto uri = change.at("uri").get<std::string_view>();
            auto type = change.at("type").get<long long>();
            if (type < 1 || type > 3)
                type = 0;
            return fs_change { uri, static_cast<fs_change_type>(type) };
        });
        ws_mngr_.did_change_watched_files(changes);
    }
    catch (const nlohmann::json::exception& j)
    {
        LOG_ERROR("Invalid didChangeWatchedFiles notification parameter: ", j.what());
    }
}

void feature_workspace_folders::send_configuration_request()
{
    static const nlohmann::json config_request_args { {
        "items",
        nlohmann::json::array_t {
            { { "section", "hlasm.diagnosticsSuppressLimit" } },
            nlohmann::json::object(),
        },
    } };
    response_->request(
        "workspace/configuration",
        config_request_args,
        [this](const nlohmann::json& params) { configuration(params); },
        [](int, [[maybe_unused]] const char* msg) {
            LOG_WARNING("Unexpected error configuration response received: ", msg);
        });
}

void feature_workspace_folders::register_file_change_notifictions()
{
    static const nlohmann::json global_patterns {
        {
            "registrations",
            nlohmann::json::array_t {
                {
                    { "id", "global_watchers" },
                    { "method", "workspace/didChangeWatchedFiles" },
                    {
                        "registerOptions",
                        {
                            {
                                "watchers",
                                nlohmann::json::array_t {
                                    { { "globPattern", "**/*" } },
                                    { { "globPattern", ".hlasmplugin/*.json" } },
                                },
                            },
                        },
                    },
                },
            },
        },
    };

    response_->request(
        "client/registerCapability",
        global_patterns,
        [](const nlohmann::json&) {},
        [](int, [[maybe_unused]] const char* msg) {
            LOG_WARNING("Error occurred while registering file watcher: ", msg);
        });
}

void feature_workspace_folders::configuration(const nlohmann::json& params) const
{
    if (params.size() != 2)
    {
        LOG_WARNING("Unexpected configuration response received.");
        return;
    }

    const auto& suppressLimit = params[0];
    parser_library::lib_config cfg;
    if (suppressLimit.is_number())
    {
        cfg.diag_supress_limit = suppressLimit.get<int64_t>();
        if (cfg.diag_supress_limit < 0)
            cfg.diag_supress_limit = 0;
    }
    const auto& full_cfg = params[1];

    ws_mngr_.configuration_changed(cfg, full_cfg.dump());
}

void feature_workspace_folders::did_change_configuration(const nlohmann::json&) { send_configuration_request(); }

std::string as_id_string(feature_workspace_folders::watcher_id id)
{
    return "watcher_" + std::to_string(static_cast<std::underlying_type_t<feature_workspace_folders::watcher_id>>(id));
}

std::shared_ptr<const void> feature_workspace_folders::add_watcher(std::string_view uri, bool r)
{
    if (!m_supports_file_change_notification_relative_pattern)
        return {};

    std::unique_lock lock(m_watcher_registrations_mutex);

    const auto matching_registration = [uri, r](const auto& w) { return w.base_uri == uri && w.recursive == r; };
    if (const auto it = std::ranges::find_if(m_watcher_registrations, matching_registration);
        it != std::ranges::end(m_watcher_registrations))
    {
        auto l = it->registration.lock();
        if (l)
            return l;
    }

    const auto id = next_watcher_id();

    auto cleaner = [this, id]() noexcept { remove_watcher(id); };
    auto new_reg = std::make_shared<utils::scope_exit<decltype(cleaner)>>(std::move(cleaner));

    m_watcher_registrations.emplace_back(std::string(uri), r, id, new_reg);

    lock.unlock();

    nlohmann::json::array_t watchers;
    utils::resource::resource_location loc(uri);
    loc.join("");
    watchers.push_back({
        {
            "globPattern",
            {
                { "baseUri", loc.get_uri() },
                { "pattern", r ? "**/*" : "*" },
            },
        },
    });
    if (auto parent = utils::resource::resource_location::join(loc, "..").lexically_normal(); parent != loc)
    {
        const auto rel = loc.lexically_relative(parent);
        const auto rel_uri = rel.get_uri();

        watchers.push_back({
            {
                "globPattern",
                {
                    { "baseUri", parent.get_uri() },
                    { "pattern", rel_uri.substr(0, rel_uri.size() - 1) },
                    { "kind", 1 | 4 },
                },
            },
        });
    }

    const nlohmann::json watcher_registration_request {
        {
            "registrations",
            nlohmann::json::array_t {
                {
                    { "id", as_id_string(id) },
                    { "method", "workspace/didChangeWatchedFiles" },
                    {
                        "registerOptions",
                        {
                            { "watchers", std::move(watchers) },
                        },
                    },
                },
            },
        },
    };

    response_->request(
        "client/registerCapability",
        watcher_registration_request,
        [](const nlohmann::json&) {},
        [this, w = std::weak_ptr(new_reg)](int, [[maybe_unused]] const char* msg) {
            std::lock_guard g(m_watcher_registrations_mutex);
            std::erase_if(m_watcher_registrations, [&w](const watcher_registration& r) {
                // TODO: C++26 owner_equal
                return !r.registration.owner_before(w) && !w.owner_before(r.registration);
            });
            LOG_WARNING("Error occurred while registering a file watcher: ", msg);
        });

    return new_reg;
}

void feature_workspace_folders::remove_watcher(watcher_id id) noexcept
{
    std::unique_lock lock(m_watcher_registrations_mutex);
    std::erase_if(m_watcher_registrations, [id](const auto& r) { return r.id == id; });
    lock.unlock();

    try
    {
        const nlohmann::json unregistration_request {
            {
                "unregistrations",
                nlohmann::json::array_t {
                    {
                        { "id", as_id_string(id) },
                        { "method", "workspace/didChangeWatchedFiles" },
                    },
                },
            },
        };

        response_->request(
            "client/unregisterCapability",
            unregistration_request,
            [](const nlohmann::json&) {},
            [](int, [[maybe_unused]] const char* msg) {
                LOG_WARNING("Error occurred while unregistering file watcher: ", msg);
            });
    }
    catch (...)
    {
        LOG_WARNING("Unable to unregister file watcher");
    }
}

constexpr feature_workspace_folders::watcher_id feature_workspace_folders::next_watcher_id() noexcept
{
    const auto n = static_cast<std::underlying_type_t<watcher_id>>(m_next_watcher_id) + 1;
    const auto w = static_cast<watcher_id>(n);
    m_next_watcher_id = w;
    return w;
}

} // namespace hlasm_plugin::language_server::lsp
