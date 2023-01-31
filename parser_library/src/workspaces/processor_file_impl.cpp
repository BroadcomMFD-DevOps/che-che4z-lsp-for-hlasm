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

#include "processor_file_impl.h"

#include <cassert>
#include <memory>
#include <string>
#include <utility>

#include "file.h"
#include "file_manager.h"
#include "processing/statement_analyzers/hit_count_analyzer.h"

namespace hlasm_plugin::parser_library::workspaces {

processor_file_impl::processor_file_impl(std::shared_ptr<file> file, file_manager& file_mngr, std::atomic<bool>* cancel)
    : file_mngr_(file_mngr)
    , file_(std::move(file))
    , cancel_(cancel)
    , macro_cache_(file_mngr, file_)
{}

void processor_file_impl::collect_diags() const {}
bool processor_file_impl::is_once_only() const { return false; }

parse_result processor_file_impl::parse(parse_lib_provider& lib_provider,
    asm_option asm_opts,
    std::vector<preprocessor_options> pp,
    virtual_file_monitor* vfm)
{
    if (!m_last_analyzer_opencode)
        m_last_opencode_id_storage = std::make_shared<context::id_storage>();

    const bool collect_hl = should_collect_hl();
    auto fms = std::make_shared<std::vector<fade_message_s>>();
    auto new_analyzer = std::make_unique<analyzer>(file_->get_text(),
        analyzer_options {
            file_->get_location(),
            &lib_provider,
            std::move(asm_opts),
            collect_hl ? collect_highlighting_info::yes : collect_highlighting_info::no,
            file_is_opencode::yes,
            m_last_opencode_id_storage,
            std::move(pp),
            vfm,
            fms,
        });

    auto old_dep = m_dependencies;

    processing::hit_count_analyzer hc_analyzer(new_analyzer->hlasm_ctx());
    new_analyzer->register_stmt_analyzer(&hc_analyzer);

    new_analyzer->analyze(cancel_);

    if (m_cancel && *m_cancel)
        return false;

    diags().clear();
    collect_diags_from_child(*new_analyzer);

    m_last_analyzer = std::move(new_analyzer);
    m_last_analyzer_opencode = true;
    m_last_analyzer_with_lsp = collect_hl;

    m_dependencies.clear();
    for (auto& file : m_last_analyzer->hlasm_ctx().get_visited_files())
        if (file != m_file->get_location())
            m_dependencies.insert(file);

    m_files_to_close.clear();
    // files that used to be dependencies but are not anymore should be closed internally
    for (const auto& file : old_dep)
    {
        if (m_dependencies.find(file) == m_dependencies.end())
            m_files_to_close.insert(file);
    }

    m_hc_map = std::move(hc_analyzer.take_hit_counts());
    m_fade_messages = std::move(fms);


    return true;
}

parse_result processor_file_impl::parse_macro(
    parse_lib_provider& lib_provider, analyzing_context ctx, library_data data)
{
    auto cache_key = macro_cache_key::create_from_context(*ctx.hlasm_ctx, data);

    if (m_macro_cache.load_from_cache(cache_key, ctx))
        return true;

    const bool collect_hl = should_collect_hl(ctx.hlasm_ctx.get());
    auto a = std::make_unique<analyzer>(file_->get_text(),
        analyzer_options {
            file_->get_location(),
            &lib_provider,
            std::move(ctx),
            data,

    processing::hit_count_analyzer hc_analyzer(a->hlasm_ctx());
    a->register_stmt_analyzer(&hc_analyzer);

    a->analyze(cancel_);


    if (cancel_ && *cancel_)
        return false;

    macro_cache_.save_macro(cache_key, *a);
    last_analyzer_ = std::move(a);

    m_macro_cache.save_macro(cache_key, *a);
    m_last_analyzer = std::move(a);
    m_hc_map.clear();
    store_hit_counts(hc_analyzer, m_hc_map);

    return true;
}

const std::set<utils::resource::resource_location>& processor_file_impl::dependencies() { return m_dependencies; }

const semantics::lines_info& processor_file_impl::get_hl_info()
{
    if (m_last_analyzer)
        return m_last_analyzer->source_processor().semantic_tokens();

    const static semantics::lines_info empty_lines;
    return empty_lines;
}

const lsp::lsp_context* processor_file_impl::get_lsp_context()
{
    if (m_last_analyzer)
        return m_last_analyzer->context().lsp_ctx.get();

    return nullptr;
}

const std::set<utils::resource::resource_location>& processor_file_impl::files_to_close() { return m_files_to_close; }

const performance_metrics& processor_file_impl::get_metrics()
{
    if (m_last_analyzer)
        return m_last_analyzer->get_metrics();
    const static performance_metrics metrics;
    return metrics;
}

void processor_file_impl::erase_cache_of_opencode(const utils::resource::resource_location& opencode_file_location)
{
    m_macro_cache.erase_cache_of_opencode(opencode_file_location);
}

bool processor_file_impl::should_collect_hl(context::hlasm_context* ctx) const
{
    // collect highlighting information in any of the following cases:
    // 1) The file is opened in the editor
    // 2) HL information was previously requested
    // 3) this macro is a top-level macro
    return file_->get_lsp_editing() || last_analyzer_with_lsp || ctx && ctx->processing_stack().parent().empty();
}

bool processor_file_impl::has_lsp_info() const { return last_analyzer_with_lsp; }

void processor_file_impl::retrieve_fade_messages(std::vector<fade_message_s>& fms) const
{
    fms.insert(std::end(fms), std::begin(*fade_messages_), std::end(*fade_messages_));
}
bool processor_file_impl::current_version() const
{
    auto f = file_mngr_.find(get_location());
    return f == file_;
}

void processor_file_impl::update_source()
{
    last_analyzer_.reset();
    used_files.clear();
    file_ = file_mngr_.add_file(get_location());
    macro_cache_ = macro_cache(file_mngr_, file_);
    diags().clear();
}

void processor_file_impl::store_used_files(std::unordered_map<utils::resource::resource_location,
    std::shared_ptr<file>,
    utils::resource::resource_location_hasher> uf)
{
    used_files = std::move(uf);
            hc_map[rl][line].r = details.r;
        }
    }
}

void processor_file_impl::retrieve_hit_counts(processing::hit_count_map& hc_map)
{
    for (const auto& [rl, stmt_hc_m] : m_hc_map)
        if (auto [stmt_hc_it, result1] = hc_map.try_emplace(rl, stmt_hc_m); !result1)
            for (const auto& [line, details] : stmt_hc_m)
                if (auto [it, result2] = stmt_hc_it->second.try_emplace(line, details); !result2)
                    it->second.count += details.count;
}

} // namespace hlasm_plugin::parser_library::workspaces
