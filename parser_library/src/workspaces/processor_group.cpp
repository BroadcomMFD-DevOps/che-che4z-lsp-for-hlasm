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

#include <algorithm>
#include <array>

#include "config/proc_grps.h"

namespace hlasm_plugin::parser_library::workspaces {

namespace {
struct translate_pp_options
{
    preprocessor_options operator()(const std::monostate&) const { return std::monostate {}; }
    preprocessor_options operator()(const config::db2_preprocessor& opt) const
    {
        return db2_preprocessor_options(opt.version);
    }
    preprocessor_options operator()(const config::cics_preprocessor& opt) const
    {
        return cics_preprocessor_options(opt.prolog, opt.epilog, opt.leasm);
    }
};

constexpr std::pair<std::string_view, instruction_set_version> instruction_set_translator[] = {
    { std::string_view("370"), instruction_set_version::_370 },
    { std::string_view("DOS"), instruction_set_version::DOS },
    { std::string_view("ESA"), instruction_set_version::ESA },
    { std::string_view("UNI"), instruction_set_version::UNI },
    { std::string_view("XA"), instruction_set_version::XA },
    { std::string_view("YOP"), instruction_set_version::YOP },
    { std::string_view("Z10"), instruction_set_version::Z10 },
    { std::string_view("Z11"), instruction_set_version::Z11 },
    { std::string_view("Z12"), instruction_set_version::Z12 },
    { std::string_view("Z13"), instruction_set_version::Z13 },
    { std::string_view("Z14"), instruction_set_version::Z14 },
    { std::string_view("Z15"), instruction_set_version::Z15 },
    { std::string_view("Z9"), instruction_set_version::Z9 },
    { std::string_view("ZOP"), instruction_set_version::ZOP },
    { std::string_view("ZS1"), instruction_set_version::ZOP },
    { std::string_view("ZS2"), instruction_set_version::YOP },
    { std::string_view("ZS3"), instruction_set_version::Z9 },
    { std::string_view("ZS4"), instruction_set_version::Z10 },
    { std::string_view("ZS5"), instruction_set_version::Z11 },
    { std::string_view("ZS6"), instruction_set_version::Z12 },
    { std::string_view("ZS7"), instruction_set_version::Z13 },
    { std::string_view("ZS8"), instruction_set_version::Z14 },
    { std::string_view("ZS9"), instruction_set_version::Z15 },
};

#if __cpp_lib_ranges
static_assert(std::ranges::is_sorted(
    instruction_set_translator, {}, &std::pair<std::string_view, instruction_set_version>::first));
#else
static_assert(std::is_sorted(std::begin(instruction_set_translator),
    std::end(instruction_set_translator),
    [](const auto& l, const auto& r) { return l.first < r.first; }));
#endif
} // namespace

processor_group::processor_group(const std::string& pg_name,
    std::string_view pg_file_name,
    const config::assembler_options& asm_options,
    const config::preprocessor_options& pp)
    : m_pg_name(pg_name)
    , m_asm_opts(translate_assembler_options(asm_options, pg_file_name))
    , m_prep_opts(std::visit(translate_pp_options {}, pp.options))
{}

instruction_set_version processor_group::find_instruction_set(std::string_view optable, std::string_view pg_file_name)
{
    auto it = std::lower_bound(std::begin(instruction_set_translator),
        std::end(instruction_set_translator),
        optable,
        [](const auto& l, const auto& r) { return l.first < r; });
    if (it == std::end(instruction_set_translator) || it->first != optable)
    {
        add_diagnostic(diagnostic_s::error_W0007(pg_file_name, m_pg_name));
        return asm_option::instr_set_default;
    }

    return it->second;
}

asm_option processor_group::translate_assembler_options(
    const config::assembler_options& asm_options, std::string_view pg_file_name)
{
    return asm_option {
        asm_options.sysparm,
        asm_options.profile,
        asm_options.optable.empty() ? asm_option::instr_set_default
                                    : find_instruction_set(asm_options.optable, pg_file_name),
        asm_options.system_id.empty() ? asm_option::system_id_default : asm_options.system_id,
    };
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
