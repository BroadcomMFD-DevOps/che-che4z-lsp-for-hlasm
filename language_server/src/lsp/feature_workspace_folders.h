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

#ifndef HLASMPLUGIN_LANGUAGESERVER_FEATURE_WORKSPACEFOLDERS_H
#define HLASMPLUGIN_LANGUAGESERVER_FEATURE_WORKSPACEFOLDERS_H

#include <memory>
#include <mutex>
#include <vector>

#include "../feature.h"
#include "workspace_manager.h"


namespace hlasm_plugin::language_server::lsp {

// Groups workspace methods, which decode incomming notifications and call methods of
// provided workspace_manager.
class feature_workspace_folders final : public feature
{
    struct watcher_registration;
    enum class watcher_id : unsigned long long;

    friend std::string as_id_string(watcher_id);

public:
    explicit feature_workspace_folders(
        parser_library::workspace_manager& ws_mngr, response_provider& response_provider);
    feature_workspace_folders(const feature_workspace_folders&) = delete;
    feature_workspace_folders(feature_workspace_folders&&) noexcept = delete;
    feature_workspace_folders& operator=(const feature_workspace_folders&) = delete;
    feature_workspace_folders& operator=(feature_workspace_folders&&) noexcept = delete;
    ~feature_workspace_folders();

    // Adds workspace/* methods to the map.
    void register_methods(std::map<std::string, method>&) override;
    // Returns workspaces capability.
    nlohmann::json register_capabilities() override;
    // Opens workspace specified in the initialize request.
    void initialize_feature(const nlohmann::json& initialise_params) override;
    void initialized() override;

    std::shared_ptr<const void> add_watcher(std::string_view uri, bool recursive);

private:
    // Handles workspace/didChangeWorkspaceFolders notification.
    void on_did_change_workspace_folders(const nlohmann::json& params);
    void did_change_watched_files(const nlohmann::json& params);

    void configuration(const nlohmann::json& params) const;
    void did_change_configuration(const nlohmann::json& params);


    // Adds all workspaces specified in the json to the workspace manager.
    void add_workspaces(const nlohmann::json& added);

    // Removes all workspaces specified in the json from the workspace manager.
    void remove_workspaces(const nlohmann::json& removed);

    void send_configuration_request();

    void register_file_change_notifictions();

    void remove_watcher(watcher_id id) noexcept;

    parser_library::workspace_manager& ws_mngr_;
    std::vector<std::pair<std::string, std::string>> m_initial_workspaces;
    std::string m_root_uri;

    bool m_supports_dynamic_file_change_notification : 1 = false;
    bool m_supports_file_change_notification_relative_pattern : 1 = false;

    watcher_id m_next_watcher_id = {};
    std::mutex m_watcher_registrations_mutex;
    std::vector<watcher_registration> m_watcher_registrations;

    [[nodiscard]] constexpr watcher_id next_watcher_id() noexcept;
};

} // namespace hlasm_plugin::language_server::lsp


#endif
