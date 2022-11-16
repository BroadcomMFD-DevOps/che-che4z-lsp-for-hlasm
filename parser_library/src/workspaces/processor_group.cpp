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

#include "processor_group.h"

#include <span>

namespace hlasm_plugin::parser_library::workspaces {

namespace {
struct translate_pp_options
{
    preprocessor_options operator()(const config::db2_preprocessor& opt) const
    {
        return db2_preprocessor_options(opt.version, opt.conditional);
    }
    preprocessor_options operator()(const config::cics_preprocessor& opt) const
    {
        return cics_preprocessor_options(opt.prolog, opt.epilog, opt.leasm);
    }
    preprocessor_options operator()(const config::endevor_preprocessor&) const
    {
        return endevor_preprocessor_options();
    }
};

std::vector<preprocessor_options> translate_pp_configs(const std::vector<config::preprocessor_options>& pp)
{
    std::vector<preprocessor_options> result;
    result.reserve(pp.size());
    std::transform(pp.begin(), pp.end(), std::back_inserter(result), [](const auto& p) {
        return std::visit(translate_pp_options {}, p.options);
    });
    return result;
}
} // namespace

processor_group::processor_group(const std::string& pg_name,
    const config::assembler_options& asm_options,
    const std::vector<config::preprocessor_options>& pp)
    : m_pg_name(pg_name)
    , m_asm_opts(asm_options)
    , m_prep_opts(translate_pp_configs(pp))
{}

void processor_group::apply_options_to(asm_option& opts) const { m_asm_opts.apply_options_to(opts); }

void processor_group::generate_suggestions(bool force)
{
    if (m_suggestions.has_value())
    {
        if (!force)
            return;
        m_suggestions->clear();
    }
    else
        m_suggestions.emplace();

    for (const auto& l : m_libs)
    {
        for (auto&& filename : l->list_files())
        {
            if (filename.size() > suggestion_limit)
                continue;
            m_suggestions->insert(std::move(filename));
        }
    }
}

std::vector<std::pair<std::string, size_t>> processor_group::suggest(std::string_view opcode, bool extended)
{
    generate_suggestions(false);

    constexpr auto process = [](std::span<std::pair<const std::string*, size_t>> input) {
        std::vector<std::pair<std::string, size_t>> result;
        for (const auto& [suggestion, distance] : input)
        {
            if (!suggestion)
                break;
            if (distance == 0) // exact match
                break;
            result.emplace_back(*suggestion, distance);
        }
        return result;
    };

    if (!extended)
    {
        auto suggestions = m_suggestions->find<3>(opcode, 3); // dist = 3 <=> 1 character swap + 1 typo
        return process(suggestions);
    }
    else
    {
        auto suggestions = m_suggestions->find<10>(opcode, 4); // one extra typo
        return process(suggestions);
    }
}

void processor_group::collect_diags() const
{
    for (auto&& lib : m_libs)
    {
        collect_diags_from_child(*lib);
    }
}

void processor_group::add_library(std::unique_ptr<library> library) { m_libs.push_back(std::move(library)); }

} // namespace hlasm_plugin::parser_library::workspaces
