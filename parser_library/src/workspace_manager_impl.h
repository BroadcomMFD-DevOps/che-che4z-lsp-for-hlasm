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

#ifndef HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_MANAGER_IMPL_H
#define HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_MANAGER_IMPL_H

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "lsp/completion_item.h"
#include "lsp/document_symbol_item.h"
#include "protocol.h"
#include "workspace_manager.h"
#include "workspace_manager_response.h"
#include "workspaces/file_manager_impl.h"
#include "workspaces/workspace.h"

namespace hlasm_plugin::parser_library {

// Implementation of workspace manager (Implementation part of the pimpl idiom)
// Holds workspaces, file manager and macro tracer and handles LSP and DAP
// notifications and requests.
class workspace_manager::impl final : public diagnosable_impl
{
    static constexpr lib_config supress_all { 0 };

public:
    impl(std::atomic<bool>* cancel = nullptr)
        : cancel_(cancel)
        , implicit_workspace_(file_manager_, global_config_, m_global_settings)
        , quiet_implicit_workspace_(file_manager_, supress_all, m_global_settings)
    {}
    impl(const impl&) = delete;
    impl& operator=(const impl&) = delete;

    impl(impl&&) = delete;
    impl& operator=(impl&&) = delete;

    static auto& ws_path_match(auto& self, std::string_view document_uri)
    {
        if (auto hlasm_id = extract_hlasm_id(document_uri); hlasm_id.has_value())
        {
            if (auto related_ws = self.file_manager_.get_virtual_file_workspace(hlasm_id.value()); !related_ws.empty())
                for (auto& [_, ws] : self.workspaces_)
                    if (ws.uri() == related_ws.get_uri())
                        return ws;
        }

        size_t max = 0;
        decltype(&self.workspaces_.begin()->second) max_ws = nullptr;
        for (auto& [name, ws] : self.workspaces_)
        {
            size_t match = prefix_match(document_uri, ws.uri());
            if (match > max && match >= name.size())
            {
                max = match;
                max_ws = &ws;
            }
        }
        if (max_ws != nullptr)
            return *max_ws;
        else if (document_uri.starts_with("file:") || document_uri.starts_with("untitled:"))
            return self.implicit_workspace_;
        else
            return self.quiet_implicit_workspace_;
    }

    // returns implicit workspace, if the file does not belong to any workspace
    auto& ws_path_match(std::string_view document_uri) { return ws_path_match(*this, document_uri); }
    auto& ws_path_match(std::string_view document_uri) const { return ws_path_match(*this, document_uri); }

    size_t get_workspaces(ws_id* workspaces, size_t max_size)
    {
        size_t size = 0;

        for (auto it = workspaces_.begin(); size < max_size && it != workspaces_.end(); ++size, ++it)
        {
            workspaces[size] = &it->second;
        }
        return size;
    }

    size_t get_workspaces_count() const { return workspaces_.size(); }

    void add_workspace(std::string name, std::string uri)
    {
        auto ws = workspaces_.emplace(name,
            workspaces::workspace(utils::resource::resource_location(std::move(uri)),
                name,
                file_manager_,
                global_config_,
                m_global_settings));
        ws.first->second.set_message_consumer(message_consumer_);
        ws.first->second.open();

        notify_diagnostics_consumers();
    }
    ws_id find_workspace(const std::string& document_uri) { return &ws_path_match(document_uri); }
    void remove_workspace(std::string uri)
    {
        auto it = workspaces_.find(uri);
        if (it == workspaces_.end())
            return; // erase does no action, if the key does not exist
        workspaces_.erase(uri);
        notify_diagnostics_consumers();
    }

    void run_parse_loop(workspaces::workspace& ws)
    {
        for (auto task = ws.parse_file(); task.valid(); task = ws.parse_file())
        {
            auto start = std::chrono::steady_clock::now();
            while (!task.done())
            {
                if (cancel_ && cancel_->load(std::memory_order_relaxed))
                    return;
                task.resume();
            }
            std::chrono::duration<double> duration = std::chrono::steady_clock::now() - start;
            const auto& [url, metadata, perf_metrics, errors, warnings] = task.value();

            if (perf_metrics)
            {
                parsing_metadata data { perf_metrics.value(), std::move(metadata), errors, warnings };
                for (auto consumer : parsing_metadata_consumers_)
                    consumer->consume_parsing_metadata(sequence<char>(url.get_uri()), duration.count(), data);
            }
        }
    }

    void run_parse_loop()
    {
        for (auto& [_, ws] : workspaces_)
            run_parse_loop(ws);
        run_parse_loop(implicit_workspace_);
        run_parse_loop(quiet_implicit_workspace_);

        notify_diagnostics_consumers();
    }

    void did_open_file(const utils::resource::resource_location& document_loc, version_t version, std::string text)
    {
        auto file_changed = file_manager_.did_open_file(document_loc, version, std::move(text));
        workspaces::workspace& ws = ws_path_match(document_loc.get_uri());
        ws.did_open_file(document_loc, file_changed);

        run_parse_loop();
    }

    void did_change_file(const utils::resource::resource_location& document_loc,
        version_t version,
        const document_change* changes,
        size_t ch_size)
    {
        file_manager_.did_change_file(document_loc, version, changes, ch_size);
        workspaces::workspace& ws = ws_path_match(document_loc.get_uri());
        ws.did_change_file(document_loc, changes, ch_size);

        run_parse_loop();
    }

    void did_close_file(const utils::resource::resource_location& document_loc)
    {
        file_manager_.did_close_file(document_loc);

        workspaces::workspace& ws = ws_path_match(document_loc.get_uri());
        ws.did_close_file(document_loc);

        run_parse_loop();
    }

    void did_change_watched_files(std::vector<utils::resource::resource_location> paths)
    {
        std::unordered_map<workspaces::workspace*, std::vector<utils::resource::resource_location>> paths_for_ws;
        for (auto& path : paths)
            paths_for_ws[&ws_path_match(path.get_uri())].push_back(std::move(path));

        for (const auto& [ws, path_list] : paths_for_ws)
            ws->did_change_watched_files(path_list);

        run_parse_loop();
    }

    void register_diagnostics_consumer(diagnostics_consumer* consumer) { diag_consumers_.push_back(consumer); }
    void unregister_diagnostics_consumer(diagnostics_consumer* consumer)
    {
        diag_consumers_.erase(
            std::remove(diag_consumers_.begin(), diag_consumers_.end(), consumer), diag_consumers_.end());
    }

    void register_parsing_metadata_consumer(parsing_metadata_consumer* consumer)
    {
        parsing_metadata_consumers_.push_back(consumer);
    }

    void unregister_parsing_metadata_consumer(parsing_metadata_consumer* consumer)
    {
        auto& pmc = parsing_metadata_consumers_;
        pmc.erase(std::remove(pmc.begin(), pmc.end(), consumer), pmc.end());
    }

    void set_message_consumer(message_consumer* consumer)
    {
        message_consumer_ = consumer;
        implicit_workspace_.set_message_consumer(consumer);
        for (auto& wks : workspaces_)
            wks.second.set_message_consumer(consumer);
    }

    void definition(const std::string& document_uri, position pos, workspace_manager_response<position_uri> r)
    {
        location definition_result;
        auto doc_loc = utils::resource::resource_location(document_uri);

        if (cancel_ && *cancel_)
        {
            definition_result = { pos, doc_loc };
            r.provide(position_uri(definition_result));
            return;
        }

        definition_result = ws_path_match(document_uri).definition(doc_loc, pos);

        r.provide(position_uri(definition_result));
    }

    void references(const std::string& document_uri, position pos, workspace_manager_response<position_uri_list> r)
    {
        if (cancel_ && *cancel_)
        {
            r.provide({});
            return;
        }

        auto references_result =
            ws_path_match(document_uri).references(utils::resource::resource_location(document_uri), pos);

        r.provide({ references_result.data(), references_result.size() });
    }

    void hover(const std::string& document_uri, position pos, workspace_manager_response<sequence<char>> r)
    {
        if (cancel_ && *cancel_)
        {
            r.provide(sequence<char>(std::string_view("")));
            return;
        }

        auto hover_result = ws_path_match(document_uri).hover(utils::resource::resource_location(document_uri), pos);

        r.provide(sequence<char>(hover_result));
    }


    void completion(const std::string& document_uri,
        position pos,
        const char trigger_char,
        completion_trigger_kind trigger_kind,
        workspace_manager_response<completion_list> r)
    {
        if (cancel_ && *cancel_)
        {
            r.provide(completion_list { nullptr, 0 });
            return;
        }

        auto completion_result =
            ws_path_match(document_uri)
                .completion(utils::resource::resource_location(document_uri), pos, trigger_char, trigger_kind);

        r.provide(completion_list(completion_result.data(), completion_result.size()));
    }

    void document_symbol(
        const std::string& document_uri, long long limit, workspace_manager_response<document_symbol_list> r)
    {
        if (cancel_ && *cancel_)
        {
            r.provide(document_symbol_list { nullptr, 0 });
            return;
        }

        auto document_symbol_result =
            ws_path_match(document_uri).document_symbol(utils::resource::resource_location(document_uri), limit);

        r.provide(document_symbol_list(document_symbol_result.data(), document_symbol_result.size()));
    }

    lib_config global_config_;
    workspaces::shared_json m_global_settings = std::make_shared<const nlohmann::json>(nlohmann::json::object());
    void configuration_changed(const lib_config& new_config, std::shared_ptr<const nlohmann::json> global_settings)
    {
        global_config_ = new_config;
        m_global_settings.store(std::move(global_settings));

        bool updated = false;
        for (auto& [_, ws] : workspaces_)
            updated |= ws.settings_updated();

        if (updated)
            notify_diagnostics_consumers();
    }

    void semantic_tokens(const char* document_uri, workspace_manager_response<continuous_sequence<token_info>> r)
    {
        r.provide(make_continuous_sequence(
            ws_path_match(document_uri).semantic_tokens(utils::resource::resource_location(document_uri))));
    }

    continuous_sequence<char> get_virtual_file_content(unsigned long long id) const
    {
        return make_continuous_sequence(file_manager_.get_virtual_file(id));
    }

    void make_opcode_suggestion(const std::string& document_uri,
        const std::string& opcode,
        bool extended,
        workspace_manager_response<continuous_sequence<opcode_suggestion>> r)
    {
        auto suggestions =
            ws_path_match(document_uri)
                .make_opcode_suggestion(utils::resource::resource_location(document_uri), opcode, extended);

        std::vector<opcode_suggestion> res;

        for (auto&& [suggestion, distance] : suggestions)
            res.emplace_back(opcode_suggestion { make_continuous_sequence(std::move(suggestion)), distance });

        r.provide(make_continuous_sequence(std::move(res)));
    }

private:
    void collect_diags() const override
    {
        collect_diags_from_child(implicit_workspace_);
        collect_diags_from_child(quiet_implicit_workspace_);
        for (auto& it : workspaces_)
            collect_diags_from_child(it.second);
    }

    static std::optional<unsigned long long> extract_hlasm_id(std::string_view uri)
    {
        static constexpr std::string_view prefix = "hlasm://";
        if (!uri.starts_with(prefix))
            return std::nullopt;
        uri.remove_prefix(prefix.size());
        if (auto slash = uri.find('/'); slash == std::string_view::npos)
            return std::nullopt;
        else
            uri = uri.substr(0, slash);

        unsigned long long result = 0;

        auto [p, err] = std::from_chars(uri.data(), uri.data() + uri.size(), result);
        if (err != std::errc() || p != uri.data() + uri.size())
            return std::nullopt;
        else
            return result;
    }

    void notify_diagnostics_consumers()
    {
        diags().clear();
        collect_diags();

        fade_messages_.clear();
        implicit_workspace_.retrieve_fade_messages(fade_messages_);
        quiet_implicit_workspace_.retrieve_fade_messages(fade_messages_);
        for (const auto& [_, ws] : workspaces_)
            ws.retrieve_fade_messages(fade_messages_);

        for (auto consumer : diag_consumers_)
            consumer->consume_diagnostics(diagnostic_list(diags().data(), diags().size()),
                fade_message_list(fade_messages_.data(), fade_messages_.size()));
    }

    static size_t prefix_match(std::string_view first, std::string_view second)
    {
        auto [f, s] = std::mismatch(first.begin(), first.end(), second.begin(), second.end());
        return static_cast<size_t>(std::min(f - first.begin(), s - second.begin()));
    }

    std::atomic<bool>* cancel_;
    workspaces::file_manager_impl file_manager_;
    std::unordered_map<std::string, workspaces::workspace> workspaces_;
    workspaces::workspace implicit_workspace_;
    workspaces::workspace quiet_implicit_workspace_;

    std::vector<diagnostics_consumer*> diag_consumers_;
    std::vector<parsing_metadata_consumer*> parsing_metadata_consumers_;
    message_consumer* message_consumer_ = nullptr;
    std::vector<fade_message_s> fade_messages_;
};
} // namespace hlasm_plugin::parser_library

#endif // !HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_MANAGER_IMPL_H
