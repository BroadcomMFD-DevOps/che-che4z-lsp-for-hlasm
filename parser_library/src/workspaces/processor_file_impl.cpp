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

bool processor_file_impl::parse(parse_lib_provider& lib_provider,
    asm_option asm_opts,
    std::vector<preprocessor_options> pp,
    virtual_file_monitor* vfm)
{
    if (!m_last_opencode_id_storage)
        m_last_opencode_id_storage = std::make_shared<context::id_storage>();

    const bool collect_hl = should_collect_hl();
    auto fms = std::make_shared<std::vector<fade_message_s>>();
    analyzer new_analyzer(m_file->get_text(),
        analyzer_options {
            m_file->get_location(),
            &lib_provider,
            std::move(asm_opts),
            collect_hl ? collect_highlighting_info::yes : collect_highlighting_info::no,
            file_is_opencode::yes,
            m_last_opencode_id_storage,
            std::move(pp),
            vfm,
            fms,
        });

    processing::hit_count_analyzer hc_analyzer(new_analyzer.hlasm_ctx());
    new_analyzer.register_stmt_analyzer(&hc_analyzer);

    for (auto a = new_analyzer.co_analyze(); !a.done(); a.resume())
    {
        if (m_cancel && m_cancel->load(std::memory_order_relaxed))
            return false;
    }

    diags().clear();
    collect_diags_from_child(new_analyzer);

    m_last_opencode_analyzer_with_lsp = collect_hl;
    m_last_results.hl_info = new_analyzer.take_semantic_tokens();
    m_last_results.lsp_context = new_analyzer.context().lsp_ctx;
    m_last_results.fade_messages = std::move(fms);
    m_last_results.metrics = new_analyzer.get_metrics();
    m_last_results.vf_handles = new_analyzer.take_vf_handles();
    m_last_results.hc_opencode_map = hc_analyzer.take_hit_count_map();

    return true;
}

const semantics::lines_info& processor_file_impl::get_hl_info() { return m_last_results.hl_info; }

const lsp::lsp_context* processor_file_impl::get_lsp_context() const { return m_last_results.lsp_context.get(); }

const performance_metrics& processor_file_impl::get_metrics() { return m_last_results.metrics; }

bool processor_file_impl::should_collect_hl(context::hlasm_context* ctx) const
{
    // collect highlighting information in any of the following cases:
    // 1) The file is opened in the editor
    // 2) HL information was previously requested
    // 3) this macro is a top-level macro
    return m_file->get_lsp_editing() || m_last_opencode_analyzer_with_lsp || m_last_macro_analyzer_with_lsp
        || ctx && ctx->processing_stack().parent().empty();
}

bool processor_file_impl::has_opencode_lsp_info() const { return m_last_opencode_analyzer_with_lsp; }
bool processor_file_impl::has_macro_lsp_info() const { return m_last_macro_analyzer_with_lsp; }

const std::vector<fade_message_s>& processor_file_impl::fade_messages() const { return *m_last_results.fade_messages; }

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
