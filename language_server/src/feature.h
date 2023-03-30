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

#ifndef HLASMPLUGIN_LANGUAGESERVER_FEATURE_H
#define HLASMPLUGIN_LANGUAGESERVER_FEATURE_H

#include <compare>
#include <functional>
#include <map>
#include <string>
#include <variant>

#include "nlohmann/json_fwd.hpp"
#include "workspace_manager.h"

namespace hlasm_plugin::language_server {

enum class telemetry_log_level
{
    NO_TELEMETRY,
    LOG_EVENT,
};

struct request_id
{
    std::variant<long, std::string> id;

    request_id()
        : id(0)
    {}
    explicit request_id(long l)
        : id(l)
    {}
    explicit request_id(std::string s)
        : id(std::move(s))
    {}

    auto operator<=>(const request_id&) const = default;

    std::string to_string() const
    {
        if (std::holds_alternative<long>(id))
            return "(" + std::to_string(std::get<long>(id)) + ")";
        else
            return "\"" + std::get<std::string>(id) + "\"";
    }
};

void to_json(nlohmann::json& j, const request_id& rid);
void from_json(const nlohmann::json& j, request_id& rid);
void from_json(const nlohmann::json& j, std::optional<request_id>& rid);

struct method
{
    std::variant<std::function<void(const nlohmann::json& params)>,
        std::function<void(const request_id& id, const nlohmann::json& params)>>
        handler;
    telemetry_log_level telemetry_level;

    bool is_notification_handler() const
    {
        return std::holds_alternative<std::function<void(const nlohmann::json& params)>>(handler);
    }
    bool is_request_handler() const
    {
        return std::holds_alternative<std::function<void(const request_id& id, const nlohmann::json& params)>>(handler);
    }
    const auto& as_notification_handler() const
    {
        return std::get<std::function<void(const nlohmann::json& params)>>(handler);
    }
    const auto& as_request_handler() const
    {
        return std::get<std::function<void(const request_id& id, const nlohmann::json& params)>>(handler);
    }
};

// Provides methods to send notification, respond to request and respond with error respond
class response_provider
{
public:
    virtual void request(const std::string& requested_method,
        const nlohmann::json& args,
        std::function<void(const nlohmann::json& params)> handler) = 0;
    virtual void respond(const request_id& id, const std::string& requested_method, const nlohmann::json& args) = 0;
    virtual void notify(const std::string& method, const nlohmann::json& args) = 0;
    virtual void respond_error(const request_id& id,
        const std::string& requested_method,
        int err_code,
        const std::string& err_message,
        const nlohmann::json& error) = 0;

    virtual void register_cancellable_request(const request_id& id, std::function<void()> cancel_handler) = 0;

protected:
    ~response_provider() = default;
};

// Abstract class for group of methods that add functionality to server.
class feature
{
public:
    // Constructs the feature with workspace_manager.
    // All the requests and notification are passed to the workspace manager
    explicit feature(parser_library::workspace_manager& ws_mngr)
        : ws_mngr_(ws_mngr)
    {}
    // Constructs the feature with workspace_manager and response_provider through which the feature can send messages.
    feature(parser_library::workspace_manager& ws_mngr, response_provider& response_provider)
        : ws_mngr_(ws_mngr)
        , response_(&response_provider)
    {}

    // Implement to add methods to server.
    void virtual register_methods(std::map<std::string, method>& methods) = 0;
    // Implement to add json object to server capabilities that are sent to LSP client
    // in the response to initialize request.
    nlohmann::json virtual register_capabilities() = 0;

    // Can be implemented to set feature specific to client capabilities that
    // are sent in initialize request.
    void virtual initialize_feature(const nlohmann::json& client_capabilities) = 0;

    // Converts LSP json representation of range into parse_library::range.
    static parser_library::range parse_range(const nlohmann::json& range_json);
    // Converts LSP json representation of position into parse_library::position.
    static parser_library::position parse_position(const nlohmann::json& position_json);

    static nlohmann::json range_to_json(const parser_library::range& range);
    static nlohmann::json position_to_json(const parser_library::position& position);

    virtual ~feature() = default;

protected:
    parser_library::workspace_manager& ws_mngr_;
    bool callbacks_registered_ = false;
    response_provider* response_ = nullptr;
};

} // namespace hlasm_plugin::language_server

#endif // !HLASMPLUGIN_LANGUAGESERVER_FEATURE_H
