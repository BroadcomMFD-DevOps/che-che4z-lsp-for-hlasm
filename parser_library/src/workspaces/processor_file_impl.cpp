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
#include "lsp/lsp_context.h"

namespace hlasm_plugin::parser_library::workspaces {

processor_file_impl::processor_file_impl(std::shared_ptr<file> file, file_manager& file_mngr, std::atomic<bool>* cancel)
    : m_file_mngr(file_mngr)
    , m_file(std::move(file))
    , m_cancel(cancel)
{}

void processor_file_impl::collect_diags() const {}
bool processor_file_impl::is_once_only() const { return false; }

utils::value_task<parsing_results> parse_file(std::shared_ptr<context::id_storage> ids,
    std::shared_ptr<file> file,
    parse_lib_provider& lib_provider,
    asm_option asm_opts,
    std::vector<preprocessor_options> pp,
    virtual_file_monitor* vfm)
{
    auto fms = std::make_shared<std::vector<fade_message_s>>();
    analyzer a(file->get_text(),
        analyzer_options {
            file->get_location(),
            &lib_provider,
            std::move(asm_opts),
            collect_highlighting_info::yes,
            file_is_opencode::yes,
            std::move(ids),
            std::move(pp),
            vfm,
            fms,
        });

    processing::hit_count_analyzer hc_analyzer(a.hlasm_ctx());
    a.register_stmt_analyzer(&hc_analyzer);

    co_await a.co_analyze();

    a.collect_diags();

    parsing_results result;
    result.diagnostics = std::move(a.diags());
    result.hl_info = a.take_semantic_tokens();
    result.lsp_context = a.context().lsp_ctx;
    result.fade_messages = std::move(fms);
    result.metrics = a.get_metrics();
    result.vf_handles = a.take_vf_handles();
    result.hc_opencode_map = hc_analyzer.take_hit_count_map();

    co_return result;
}

bool processor_file_impl::parse(parse_lib_provider& lib_provider,
    asm_option asm_opts,
    std::vector<preprocessor_options> pp,
    virtual_file_monitor* vfm)
{
    if (!m_last_opencode_id_storage)
        m_last_opencode_id_storage = std::make_shared<context::id_storage>();

    auto parser = parse_file(m_last_opencode_id_storage, m_file, lib_provider, std::move(asm_opts), std::move(pp), vfm);

    while (!parser.done())
    {
        if (m_cancel && m_cancel->load(std::memory_order_relaxed))
            return false;
        parser.resume();
    }

    m_last_results = std::move(parser).value();

    m_last_opencode_analyzer_with_lsp = true;

    diags() = std::move(m_last_results.diagnostics);

    return true;
}

const semantics::lines_info& processor_file_impl::get_hl_info() { return m_last_results.hl_info; }

const lsp::lsp_context* processor_file_impl::get_lsp_context() const { return m_last_results.lsp_context.get(); }

const performance_metrics& processor_file_impl::get_metrics() { return m_last_results.metrics; }

bool processor_file_impl::has_opencode_lsp_info() const { return m_last_opencode_analyzer_with_lsp; }
bool processor_file_impl::has_macro_lsp_info() const { return m_last_macro_analyzer_with_lsp; }

const std::vector<fade_message_s>& processor_file_impl::fade_messages() const
{
    static const std::vector<fade_message_s> empty_fade_messages;
    return m_last_results.fade_messages ? *m_last_results.fade_messages : empty_fade_messages;
}

const processing::hit_count_map& processor_file_impl::hit_count_opencode_map() const
{
    return m_last_results.hc_opencode_map;
}

const processing::hit_count_map& processor_file_impl::hit_count_macro_map() const
{
    return m_last_results.hc_macro_map;
}

const utils::resource::resource_location& processor_file_impl::get_location() const { return m_file->get_location(); }

bool processor_file_impl::current_version() const { return m_file->up_to_date(); }

void processor_file_impl::update_source()
{
    m_last_results = {};
    m_file = m_file_mngr.add_file(get_location());
    diags().clear();
}

} // namespace hlasm_plugin::parser_library::workspaces
