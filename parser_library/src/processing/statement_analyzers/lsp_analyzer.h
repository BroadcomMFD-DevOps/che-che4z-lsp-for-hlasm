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

#ifndef PROCESSING_LSP_ANALYZER_H
#define PROCESSING_LSP_ANALYZER_H

#include <array>
#include <map>
#include <string_view>

#include "context/common_types.h"
#include "context/copy_member.h"
#include "context/macro.h"
#include "lsp/macro_info.h"
#include "lsp/symbol_occurrence.h"
#include "processing/processing_format.h"
#include "processing/statement_providers/statement_provider_kind.h"
#include "statement_analyzer.h"

namespace hlasm_plugin::parser_library {
struct range;
} // namespace hlasm_plugin::parser_library

namespace hlasm_plugin::parser_library::context {
class hlasm_context;
class id_index;
} // namespace hlasm_plugin::parser_library::context

namespace hlasm_plugin::parser_library::lsp {
class lsp_context;
} // namespace hlasm_plugin::parser_library::lsp

namespace hlasm_plugin::parser_library::processing {
struct copy_processing_result;
struct macrodef_processing_result;
struct macrodef_start_data;
struct resolved_statement;

class occurrence_collector;
} // namespace hlasm_plugin::parser_library::processing

namespace hlasm_plugin::parser_library::semantics {
struct deferred_operands_si;
struct instruction_si;
struct label_si;
struct operands_si;
struct preprocessor_statement_si;
struct variable_symbol;
} // namespace hlasm_plugin::parser_library::semantics

namespace hlasm_plugin::parser_library::workspaces {
class parse_lib_provider;
} // namespace hlasm_plugin::parser_library::workspaces

namespace hlasm_plugin::utils::resource {
class resource_location;
} // namespace hlasm_plugin::utils::resource

namespace hlasm_plugin::parser_library::processing {

class lsp_analyzer : public statement_analyzer
{
    context::hlasm_context& hlasm_ctx_;
    lsp::lsp_context& lsp_ctx_;
    // text of the file this analyzer is assigned to
    std::string_view file_text_;

    bool in_macro_ = false;
    size_t macro_nest_ = 1;
    lsp::file_occurrences_t macro_occurrences_;

    lsp::file_occurrences_t opencode_occurrences_;
    lsp::vardef_storage opencode_var_defs_;

    std::vector<lsp::symbol_occurrence>* stmt_occurrences_ = nullptr;
    size_t stmt_occurrences_last_ = 0;
    std::map<size_t, size_t>* stmt_ranges_ = nullptr;

public:
    lsp_analyzer(context::hlasm_context& hlasm_ctx, lsp::lsp_context& lsp_ctx, std::string_view file_text);

    bool analyze(const context::hlasm_statement& statement,
        statement_provider_kind prov_kind,
        processing_kind proc_kind,
        bool evaluated_model) override;

    void analyze_aread_line(const utils::resource::resource_location&, size_t, std::string_view) override {}

    void analyze(const semantics::preprocessor_statement_si& statement);

    void macrodef_started(const macrodef_start_data& data);
    void macrodef_finished(context::macro_def_ptr macrodef, macrodef_processing_result&& result);

    void copydef_finished(context::copy_member_ptr copydef, copy_processing_result&& result);

    void opencode_finished(workspaces::parse_lib_provider& libs);

private:
    void assign_statement_occurrences(const utils::resource::resource_location& doc_location);

    void collect_occurrences(
        lsp::occurrence_kind kind, const context::hlasm_statement& statement, bool evaluated_model);
    void collect_occurrences(lsp::occurrence_kind kind, const semantics::preprocessor_statement_si& statement);

    void collect_occurrence(const semantics::label_si& label, occurrence_collector& collector);
    void collect_occurrence(const semantics::instruction_si& instruction, occurrence_collector& collector);
    void collect_occurrence(const semantics::operands_si& operands, occurrence_collector& collector);
    void collect_occurrence(const semantics::deferred_operands_si& operands, occurrence_collector& collector);

    void collect_var_definition(const processing::resolved_statement& statement);
    void collect_copy_operands(const processing::resolved_statement& statement, bool evaluated_model);

    void collect_SET_defs(const processing::resolved_statement& statement, context::SET_t_enum type);
    void collect_LCL_GBL_defs(const processing::resolved_statement& statement, context::SET_t_enum type, bool global);
    void add_var_def(const semantics::variable_symbol* var, context::SET_t_enum type, bool global);

    void add_copy_operand(context::id_index name, const range& operand_range, bool evaluated_model);

    void update_macro_nest(const processing::resolved_statement& statement);

    bool is_LCL_GBL(const processing::resolved_statement& statement, context::SET_t_enum& set_type, bool& global) const;
    bool is_SET(const processing::resolved_statement& statement, context::SET_t_enum& set_type) const;
};

} // namespace hlasm_plugin::parser_library::processing

#endif
