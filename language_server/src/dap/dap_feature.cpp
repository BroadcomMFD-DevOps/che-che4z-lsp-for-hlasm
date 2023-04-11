/*
 * Copyright (c) 2021 Broadcom.
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

#include "dap_feature.h"

#include "nlohmann/json.hpp"
#include "utils/path.h"
#include "utils/path_conversions.h"
#include "utils/platform.h"
#include "workspace_manager_response.h"

namespace {
using namespace hlasm_plugin::language_server::dap;

std::string server_conformant_path(std::string_view path, path_format path_format)
{
    // Server accepts paths in URI format
    if (path_format == path_format::URI)
        return std::string(path);

    // Theia sends us relative path while not accepting it back. Change to absolute
    std::filesystem::path p = hlasm_plugin::utils::path::absolute(path);

    std::string result = hlasm_plugin::utils::path::lexically_normal(p).string();

    // On windows, VS code sends us path with capital drive letter through DAP and
    // lowercase drive letter through LSP.
    // Remove, once we implement case-insensitive comparison of paths in parser_library for windows
    if (hlasm_plugin::utils::platform::is_windows())
    {
        if (result[1] == ':')
            result[0] = (char)tolower((unsigned char)result[0]);
    }

    return hlasm_plugin::utils::path::path_to_uri(result);
}

std::string client_conformant_path(std::string_view uri, path_format client_path_format)
{
    // Server provides paths in URI format -> convert it to whatever the client wants
    if (client_path_format == path_format::URI)
        return std::string(uri);

    auto generated_path = hlasm_plugin::utils::path::uri_to_path(uri);
    return generated_path.empty() ? std::string(uri) : generated_path;
}

constexpr const int THREAD_ID = 1;

} // namespace

namespace hlasm_plugin::language_server::dap {
void dap_feature::initialize_feature(const nlohmann::json&)
{ /* nothing to do */
}

dap_feature::dap_feature(parser_library::workspace_manager& ws_mngr,
    response_provider& response_provider,
    dap_disconnect_listener* disconnect_listener)
    : feature(ws_mngr, response_provider)
    , disconnect_listener_(disconnect_listener)
{}


void dap_feature::register_methods(std::map<std::string, method>& methods)
{
    const auto this_bind = [this](void (dap_feature::*func)(const request_id&, const nlohmann::json&),
                               telemetry_log_level telem = telemetry_log_level::NO_TELEMETRY) {
        return method {
            [this, func](
                const request_id& requested_seq, const nlohmann::json& args) { (this->*func)(requested_seq, args); },
            telem,
        };
    };
    methods.try_emplace("initialize", this_bind(&dap_feature::on_initialize));
    methods.try_emplace("disconnect", this_bind(&dap_feature::on_disconnect, telemetry_log_level::LOG_EVENT));
    methods.try_emplace("launch", this_bind(&dap_feature::on_launch, telemetry_log_level::LOG_EVENT));
    methods.try_emplace("setBreakpoints", this_bind(&dap_feature::on_set_breakpoints, telemetry_log_level::LOG_EVENT));
    methods.try_emplace("setExceptionBreakpoints",
        this_bind(&dap_feature::on_set_exception_breakpoints, telemetry_log_level::LOG_EVENT));
    methods.try_emplace("configurationDone", this_bind(&dap_feature::on_configuration_done));
    methods.try_emplace("threads", this_bind(&dap_feature::on_threads));
    methods.try_emplace("stackTrace", this_bind(&dap_feature::on_stack_trace));
    methods.try_emplace("scopes", this_bind(&dap_feature::on_scopes));
    methods.try_emplace("next", this_bind(&dap_feature::on_next, telemetry_log_level::LOG_EVENT));
    methods.try_emplace("stepIn", this_bind(&dap_feature::on_step_in, telemetry_log_level::LOG_EVENT));
    methods.try_emplace("stepOut", this_bind(&dap_feature::on_step_out, telemetry_log_level::LOG_EVENT));
    methods.try_emplace("variables", this_bind(&dap_feature::on_variables));
    methods.try_emplace("continue", this_bind(&dap_feature::on_continue, telemetry_log_level::LOG_EVENT));
    methods.try_emplace("pause", this_bind(&dap_feature::on_pause, telemetry_log_level::LOG_EVENT));
}
nlohmann::json dap_feature::register_capabilities() { return nlohmann::json(); }

void dap_feature::stopped(
    hlasm_plugin::parser_library::sequence<char> reason, hlasm_plugin::parser_library::sequence<char>)
{
    response_->notify("stopped",
        nlohmann::json {
            { "reason", std::string_view(reason) },
            { "threadId", THREAD_ID },
            { "allThreadsStopped", true },
        });
}

void dap_feature::exited(int exit_code)
{
    response_->notify("exited", nlohmann::json { { "exitCode", exit_code } });
    response_->notify("terminated", nlohmann::json());
}

void dap_feature::on_initialize(const request_id& requested_seq, const nlohmann::json& args)
{
    response_->respond(requested_seq, "initialize", nlohmann::json { { "supportsConfigurationDoneRequest", true } });

    line_1_based_ = args["linesStartAt1"].get<bool>() ? 1 : 0;
    column_1_based_ = args["columnsStartAt1"].get<bool>() ? 1 : 0;
    client_path_format_ = args["pathFormat"].get<std::string>() == "path" ? path_format::PATH : path_format::URI;

    debugger.emplace();

    response_->notify("initialized", nlohmann::json());
}

void dap_feature::on_disconnect(const request_id& request_seq, const nlohmann::json&)
{
    if (disconnect_listener_)
        disconnect_listener_->disconnected();

    debugger.reset();

    response_->respond(request_seq, "disconnect", nlohmann::json());
}

void dap_feature::on_launch(const request_id& request_seq, const nlohmann::json& args)
{
    if (!debugger)
        return;

    // wait for configurationDone?
    std::string program_path = server_conformant_path(args["program"].get<std::string_view>(), client_path_format_);
    bool stop_on_entry = args["stopOnEntry"].get<bool>();
    auto workspace_id = ws_mngr_.find_workspace(program_path.c_str());
    debugger->set_event_consumer(this);

    struct launch_handler
    {
        request_id rs;
        response_provider* rp;

        void provide(bool launched) const
        {
            if (launched)
                rp->respond(rs, "launch", nlohmann::json());
            else
                rp->respond_error(rs, "launch", 0, "File not found", nlohmann::json());
        }

        void error(int err, const char* msg) const { rp->respond_error(rs, "launch", err, msg, nlohmann::json()); }
    };

    debugger->launch(program_path.c_str(),
        *workspace_id,
        stop_on_entry,
        parser_library::make_workspace_manager_response(launch_handler { request_seq, response_ }).first);
}

void dap_feature::on_set_breakpoints(const request_id& request_seq, const nlohmann::json& args)
{
    if (!debugger)
        return;

    nlohmann::json breakpoints_verified = nlohmann::json::array();

    std::string source = server_conformant_path(args["source"]["path"].get<std::string_view>(), client_path_format_);
    std::vector<parser_library::breakpoint> breakpoints;

    if (auto bpoints_found = args.find("breakpoints"); bpoints_found != args.end())
    {
        for (auto& bp_json : bpoints_found.value())
        {
            breakpoints.emplace_back(bp_json["line"].get<nlohmann::json::number_unsigned_t>() - line_1_based_);
            breakpoints_verified.push_back(nlohmann::json { { "verified", true } });
        }
    }
    debugger->breakpoints(source, hlasm_plugin::parser_library::sequence(breakpoints));

    response_->respond(request_seq, "setBreakpoints", nlohmann::json { { "breakpoints", breakpoints_verified } });
}

void dap_feature::on_set_exception_breakpoints(const request_id& request_seq, const nlohmann::json&)
{
    response_->respond(request_seq, "setExceptionBreakpoints", nlohmann::json());
}

void dap_feature::on_configuration_done(const request_id& request_seq, const nlohmann::json&)
{
    response_->respond(request_seq, "configurationDone", nlohmann::json());
}

void dap_feature::on_threads(const request_id& request_seq, const nlohmann::json&)
{
    response_->respond(request_seq,
        "threads",
        nlohmann::json { {
            "threads",
            nlohmann::json::array({
                {
                    { "id", THREAD_ID },
                    { "name", "main" },
                },
            }),
        } });
}

[[nodiscard]] nlohmann::json source_to_json(parser_library::source source, path_format path_format)
{
    return nlohmann::json { { "path", client_conformant_path(std::string_view(source.uri), path_format) } };
}

void dap_feature::on_stack_trace(const request_id& request_seq, const nlohmann::json&)
{
    if (!debugger)
        return;

    nlohmann::json frames_json = nlohmann::json::array();

    for (const auto& frame : debugger->stack_frames())
    {
        frames_json.push_back(nlohmann::json {
            { "id", frame.id },
            { "name", std::string_view(frame.name) },
            { "source", source_to_json(frame.source_file, client_path_format_) },
            { "line", frame.source_range.start.line + line_1_based_ },
            { "column", frame.source_range.start.column + column_1_based_ },
            { "endLine", frame.source_range.end.line + line_1_based_ },
            { "endColumn", frame.source_range.end.column + column_1_based_ },
        });
    }


    response_->respond(request_seq,
        "stackTrace",
        nlohmann::json {
            { "stackFrames", frames_json },
            { "totalFrames", frames_json.size() },
        });
}

void dap_feature::on_scopes(const request_id& request_seq, const nlohmann::json& args)
{
    if (!debugger)
        return;

    nlohmann::json scopes_json = nlohmann::json::array();

    for (const auto& s : debugger->scopes(args["frameId"].get<nlohmann::json::number_unsigned_t>()))
    {
        auto scope = parser_library::scope(s);
        auto scope_json = nlohmann::json {
            { "name", std::string_view(scope.name) },
            { "variablesReference", scope.variable_reference },
            { "expensive", false },
            { "source", source_to_json(scope.source_file, client_path_format_) },
        };
        scopes_json.push_back(std::move(scope_json));
    }


    response_->respond(request_seq, "scopes", nlohmann::json { { "scopes", scopes_json } });
}

void dap_feature::on_next(const request_id& request_seq, const nlohmann::json&)
{
    if (!debugger)
        return;

    debugger->next();

    response_->respond(request_seq, "next", nlohmann::json());
}

void dap_feature::on_step_in(const request_id& request_seq, const nlohmann::json&)
{
    if (!debugger)
        return;

    debugger->step_in();
    response_->respond(request_seq, "stepIn", nlohmann::json());
}

void dap_feature::on_step_out(const request_id& request_seq, const nlohmann::json&)
{
    if (!debugger)
        return;

    debugger->step_out();
    response_->respond(request_seq, "stepOut", nlohmann::json());
}

void dap_feature::on_variables(const request_id& request_seq, const nlohmann::json& args)
{
    if (!debugger)
        return;

    nlohmann::json variables_json = nlohmann::json::array();

    for (auto var : debugger->variables(parser_library::var_reference_t(args["variablesReference"])))
    {
        std::string type;
        switch (var.type)
        {
            case parser_library::set_type::A_TYPE:
                type = "A_TYPE";
                break;
            case parser_library::set_type::B_TYPE:
                type = "B_TYPE";
                break;
            case parser_library::set_type::C_TYPE:
                type = "C_TYPE";
                break;
            default:
                break;
        }

        nlohmann::json var_json;
        if (type == "")
            var_json = nlohmann::json {
                { "name", std::string_view(var.name) },
                { "value", std::string_view(var.value) },
                { "variablesReference", var.variable_reference },
            };
        else
            var_json = nlohmann::json { { "name", std::string_view(var.name) },
                { "value", std::string_view(var.value) },
                { "variablesReference", var.variable_reference },
                { "type", type } };

        variables_json.push_back(std::move(var_json));
    }

    response_->respond(request_seq, "variables", nlohmann::json { { "variables", variables_json } });
}

void dap_feature::on_continue(const request_id& request_seq, const nlohmann::json&)
{
    if (!debugger)
        return;

    debugger->continue_debug();

    response_->respond(request_seq, "continue", nlohmann::json { { "allThreadsContinued", true } });
}

void dap_feature::on_pause(const request_id& request_seq, const nlohmann::json&)
{
    if (!debugger)
        return;

    debugger->pause();

    response_->respond(request_seq, "pause", nlohmann::json());
}

bool dap_feature::idle_handler(const std::atomic<unsigned char>* yield_indicator)
{
    return debugger && debugger->analysis_step(yield_indicator);
}

} // namespace hlasm_plugin::language_server::dap
