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

constexpr auto sys_arch_translator = []() {
    constexpr std::array sys_arch_equivalents { std::make_pair(std::string_view("ZS1"), system_architecture::ZS1),
        std::make_pair(std::string_view("ZS2"), system_architecture::ZS2),
        std::make_pair(std::string_view("ZS3"), system_architecture::ZS3),
        std::make_pair(std::string_view("ZS4"), system_architecture::ZS4),
        std::make_pair(std::string_view("ZS5"), system_architecture::ZS5),
        std::make_pair(std::string_view("ZS6"), system_architecture::ZS6),
        std::make_pair(std::string_view("ZS7"), system_architecture::ZS7),
        std::make_pair(std::string_view("ZS8"), system_architecture::ZS8),
        std::make_pair(std::string_view("ZS9"), system_architecture::ZS9),
        std::make_pair(std::string_view("UNI"), system_architecture::UNI),
        std::make_pair(std::string_view("DOS"), system_architecture::DOS),
        std::make_pair(std::string_view("370"), system_architecture::_370),
        std::make_pair(std::string_view("XA"), system_architecture::XA),
        std::make_pair(std::string_view("ESA"), system_architecture::ESA) };

    auto temp = sys_arch_equivalents;
    std::sort(std::begin(temp), std::end(temp), [](const auto& l, const auto& r) { return l.first < r.first; });
    return temp;
}();

} // namespace

processor_group::processor_group(const std::string& pg_name,
    std::string_view pg_file_name,
    const config::assembler_options& asm_options,
    const config::preprocessor_options& pp)
    : pg_name_(pg_name)
    , pg_file_name_(pg_file_name)
    , asm_opts_(translate_assembler_options(asm_options))
    , prep_opts_(std::visit(translate_pp_options {}, pp.options))
{}

system_architecture processor_group::find_system_architecture(std::string_view arch)
{
    auto it = std::lower_bound(
        std::begin(sys_arch_translator), std::end(sys_arch_translator), arch, [](const auto& l, const auto& r) {
            return l.first < r;
        });
    if (it == std::end(sys_arch_translator) || it->first != arch)
    {
        add_diagnostic(diagnostic_s::error_W0006(pg_file_name_, pg_name_));
        return asm_option::arch_default;
    }

    return it->second;
}

asm_option processor_group::translate_assembler_options(const config::assembler_options& asm_options)
{
    return asm_option {
        asm_options.sysparm,
        asm_options.profile,
        asm_options.system_architecture.empty() ? asm_option::arch_default
                                                : find_system_architecture(asm_options.system_architecture),
        asm_options.system_id.empty() ? asm_option::system_id_default : asm_options.system_id,
    };
}

void processor_group::collect_diags() const
{
    for (auto&& lib : libs_)
    {
        collect_diags_from_child(*lib);
    }
}

void processor_group::add_library(std::unique_ptr<library> library) { libs_.push_back(std::move(library)); }

} // namespace hlasm_plugin::parser_library::workspaces
