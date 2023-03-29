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


#include "lsp_server.h"

#include <algorithm>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>

#include "../logger.h"
#include "feature_language_features.h"
#include "feature_text_synchronization.h"
#include "feature_workspace_folders.h"
#include "lib_config.h"
#include "nlohmann/json.hpp"
#include "parsing_metadata_serialization.h"
#include "utils/general_hashers.h"
#include "utils/resource_location.h"

namespace hlasm_plugin::language_server::lsp {

server::server(parser_library::workspace_manager& ws_mngr)
    : language_server::server(ws_mngr, this)
{
    features_.push_back(std::make_unique<feature_workspace_folders>(ws_mngr_, *this));
    features_.push_back(std::make_unique<feature_text_synchronization>(ws_mngr_, *this));
    features_.push_back(std::make_unique<feature_language_features>(ws_mngr_, *this));
    register_feature_methods();
    register_methods();

    ws_mngr_.register_diagnostics_consumer(this);
    ws_mngr_.set_message_consumer(this);

    ws_mngr_.register_parsing_metadata_consumer(this);
}

void server::consume_parsing_metadata(
    parser_library::sequence<char>, double duration, const parser_library::parsing_metadata& metadata)
{
    if (!telemetry_provider_)
        return;

    telemetry_info info {
        "parsing",
        duration,
        telemetry_metrics_info { metadata },
    };

    telemetry_provider_->send_telemetry(info);
}

void server::message_received(const nlohmann::json& message)
{
    auto id_found = message.find("id");


    auto result_found = message.find("result");
    auto error_result_found = message.find("error");

    if (result_found != message.end())
    {
        // we received a response to our request that was successful
        if (id_found == message.end())
        {
            LOG_WARNING("A response with no id field received.");
            send_telemetry_error("lsp_server/response_no_id");
            return;
        }

        auto handler_found = request_handlers_.end();
        if (id_found->is_number_unsigned())
            handler_found = request_handlers_.find(id_found->get<unsigned long long>());
        if (handler_found == request_handlers_.end())
        {
            LOG_WARNING("A response with no registered handler received.");
            send_telemetry_error("lsp_server/response_no_handler");
            return;
        }

        method handler = std::move(handler_found->second);
        request_handlers_.erase(handler_found);
        handler.handler(id_found.value(), result_found.value());
        return;
    }
    else if (error_result_found != message.end())
    {
        auto message_found = error_result_found->find("message");
        std::string warn_message;
        if (message_found != error_result_found->end())
            warn_message = message_found->dump();
        else
            warn_message = "Request with id " + id_found->dump() + " returned with unspecified error.";
        LOG_WARNING(warn_message);
        send_telemetry_error("lsp_server/response_error_returned", warn_message);
        return;
    }

    auto params_found = message.find("params");
    auto method_found = message.find("method");

    if (method_found == message.end())
    {
        LOG_WARNING("Method missing from received request or notification");
        send_telemetry_error("lsp_server/method_missing");
        return;
    }

    try
    {
        call_method(method_found.value().get<std::string>(),
            id_found == message.end() ? nlohmann::json() : id_found.value(),
            params_found == message.end() ? nlohmann::json() : params_found.value());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR(e.what());
        send_telemetry_error("lsp_server/method_unknown_error");
        return;
    }
}

void server::request(const std::string& requested_method, const nlohmann::json& args, method handler)
{
    auto id = request_id_counter++;
    nlohmann::json reply {
        { "jsonrpc", "2.0" },
        { "id", id },
        { "method", requested_method },
        { "params", args },
    };
    request_handlers_.emplace(id, handler);
    send_message_->reply(reply);
}

void server::respond(const nlohmann::json& id, const std::string&, const nlohmann::json& args)
{
    nlohmann::json reply {
        { "jsonrpc", "2.0" },
        { "id", id },
        { "result", args },
    };
    send_message_->reply(reply);
}

void server::notify(const std::string& method, const nlohmann::json& args)
{
    nlohmann::json reply {
        { "jsonrpc", "2.0" },
        { "method", method },
        { "params", args },
    };
    send_message_->reply(reply);
}

void server::respond_error(const nlohmann::json& id,
    const std::string&,
    int err_code,
    const std::string& err_message,
    const nlohmann::json& error)
{
    nlohmann::json reply { { "jsonrpc", "2.0" },
        { "id", id },
        { "error",
            {
                { "code", err_code },
                { "message", err_message },
                { "data", error },
            } } };
    send_message_->reply(reply);
}

void server::register_methods()
{
    methods_.try_emplace("initialize",
        method { [this](const nlohmann::json& id, const nlohmann::json& params) { on_initialize(id, params); },
            telemetry_log_level::LOG_EVENT });
    methods_.try_emplace("initialized",
        method { [](const nlohmann::json&,
                     const nlohmann::json&) { /*no implementation, silences uninteresting telemetry*/ },
            telemetry_log_level::NO_TELEMETRY });
    methods_.try_emplace("shutdown",
        method { [this](const nlohmann::json& id, const nlohmann::json& params) { on_shutdown(id, params); },
            telemetry_log_level::NO_TELEMETRY });
    methods_.try_emplace("exit",
        method { [this](const nlohmann::json& id, const nlohmann::json& params) { on_exit(id, params); },
            telemetry_log_level::NO_TELEMETRY });
    methods_.try_emplace("$/cancelRequest",
        method { [](const nlohmann::json&, const nlohmann::json&) {
                    /*no implementation, silences telemetry reporting*/
                },
            telemetry_log_level::NO_TELEMETRY });
}

void server::send_telemetry(const telemetry_message& message) { notify("telemetry/event", nlohmann::json(message)); }

void empty_handler(nlohmann::json, const nlohmann::json&)
{
    // Does nothing
}

void server::on_initialize(nlohmann::json id, const nlohmann::json& param)
{
    // send server capabilities back
    auto capabilities = nlohmann::json {
        {
            "capabilities",
            {
                { "documentFormattingProvider", false },
                { "documentRangeFormattingProvider", false },
                { "codeActionProvider", false },
                { "signatureHelpProvider", false },
                { "documentHighlightProvider", false },
                { "renameProvider", false },
                { "workspaceSymbolProvider", false },
            },
        },
    };

    for (auto& f : features_)
    {
        auto feature_cap = f->register_capabilities();
        capabilities["capabilities"].insert(feature_cap.begin(), feature_cap.end());
    }

    respond(id, "", capabilities);

    nlohmann::json register_configuration_changed_args {
        { {
            "registrations",
            {
                {
                    { "id", "configureRegister" },
                    { "method", "workspace/didChangeConfiguration" },
                },
            },
        } },
    };

    request("client/registerCapability",
        register_configuration_changed_args,
        { &empty_handler, telemetry_log_level::NO_TELEMETRY });


    for (auto& f : features_)
    {
        f->initialize_feature(param);
    }
}

void server::on_shutdown(nlohmann::json id, const nlohmann::json&)
{
    shutdown_request_received_ = true;

    // perform shutdown
    respond(id, "", nlohmann::json());
}

void server::on_exit(nlohmann::json, const nlohmann::json&) { exit_notification_received_ = true; }

void server::show_message(const std::string& message, parser_library::message_type type)
{
    nlohmann::json m {
        { "type", (int)type },
        { "message", message },
    };
    notify("window/showMessage", m);
}

nlohmann::json diagnostic_related_info_to_json(const parser_library::diagnostic& diag)
{
    nlohmann::json related;
    for (size_t i = 0; i < diag.related_info_size(); ++i)
    {
        related.push_back(nlohmann::json {
            {
                "location",
                {
                    { "uri", diag.related_info(i).location().uri() },
                    { "range", feature::range_to_json(diag.related_info(i).location().get_range()) },
                },
            },
            { "message", diag.related_info(i).message() },
        });
    }
    return related;
}

namespace {
std::string replace_empty_by_space(std::string s)
{
    if (s.empty())
        return " ";
    else
        return s;
}

nlohmann::json create_diag_json(const parser_library::range& r,
    const char* code,
    const char* source,
    const char* message,
    std::optional<nlohmann::json> diag_related_info,
    parser_library::diagnostic_severity severity,
    parser_library::diagnostic_tag tags)
{
    nlohmann::json one_json {
        { "range", feature::range_to_json(r) },
        { "code", code },
        { "source", source },
        { "message", replace_empty_by_space(message) },
    };

    if (diag_related_info.has_value())
        one_json["relatedInformation"] = std::move(*diag_related_info);

    if (severity != parser_library::diagnostic_severity::unspecified)
        one_json["severity"] = static_cast<int>(severity);

    if (tags != parser_library::diagnostic_tag::none)
    {
        auto& j_tags = one_json["tags"] = nlohmann::json::array();
        if (static_cast<int>(tags) & static_cast<int>(parser_library::diagnostic_tag::unnecessary))
            j_tags.push_back(1);
        if (static_cast<int>(tags) & static_cast<int>(parser_library::diagnostic_tag::deprecated))
            j_tags.push_back(2);
    }

    return one_json;
}
} // namespace

void server::consume_diagnostics(
    parser_library::diagnostic_list diagnostics, parser_library::fade_message_list fade_messages)
{
    std::unordered_map<std::string, nlohmann::json::array_t, utils::hashers::string_hasher, std::equal_to<>> diag_jsons;

    for (size_t i = 0; i < diagnostics.diagnostics_size(); ++i)
    {
        const auto& d = diagnostics.diagnostics(i);

        diag_jsons[d.file_uri()].emplace_back(create_diag_json(d.get_range(),
            d.code(),
            d.source(),
            d.message(),
            diagnostic_related_info_to_json(d),
            d.severity(),
            d.tags()));
    }

    for (size_t i = 0; i < fade_messages.size(); ++i)
    {
        const auto& fm = fade_messages.message(i);

        diag_jsons[fm.file_uri()].emplace_back(create_diag_json(fm.get_range(),
            fm.code(),
            fm.source(),
            fm.message(),
            std::nullopt,
            parser_library::diagnostic_severity::hint,
            parser_library::diagnostic_tag::unnecessary));
    }

    // set of all files for which diagnostics came from the server.
    std::unordered_set<std::string> new_files;
    // transform the diagnostics into json
    for (auto& [uri, diag_json] : diag_jsons)
    {
        nlohmann::json publish_diags_params {
            { "uri", uri },
            { "diagnostics", std::move(diag_json) },
        };
        new_files.insert(uri);
        last_diagnostics_files_.erase(uri);

        notify("textDocument/publishDiagnostics", publish_diags_params);
    }

    // for each file that had at least one diagnostic in the previous call of this function,
    // but does not have any diagnostics in this call, we send empty diagnostics array to
    // remove the diags from UI
    for (auto& it : last_diagnostics_files_)
    {
        nlohmann::json publish_diags_params {
            { "uri", it },
            { "diagnostics", nlohmann::json::array() },
        };
        notify("textDocument/publishDiagnostics", publish_diags_params);
    }

    last_diagnostics_files_ = std::move(new_files);
}

} // namespace hlasm_plugin::language_server::lsp
