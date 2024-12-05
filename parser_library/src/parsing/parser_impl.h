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

#ifndef HLASMPLUGIN_PARSERLIBRARY_PARSER_IMPL_H
#define HLASMPLUGIN_PARSERLIBRARY_PARSER_IMPL_H

#include <type_traits>

#include "semantics/collector.h"
#include "semantics/range_provider.h"

namespace hlasm_plugin::parser_library::context {
class hlasm_context;
} // namespace hlasm_plugin::parser_library::context

namespace hlasm_plugin::parser_library::lexing {
class lexer;
struct u8string_view_with_newlines;
} // namespace hlasm_plugin::parser_library::lexing

namespace hlasm_plugin::parser_library::parsing {

using self_def_t = std::int32_t;

struct macop_preprocess_results
{
    std::string text;
    std::vector<range> text_ranges;
    range total_op_range;
    std::vector<range> remarks;
};

// structure containing parser components
struct parser_holder
{
    context::hlasm_context* hlasm_ctx = nullptr; // TODO: notnull
    diagnostic_op_consumer* diagnostic_collector = nullptr;
    semantics::range_provider range_prov;
    std::optional<processing::processing_status> proc_status;

    std::unique_ptr<lexing::lexer> lex;
    semantics::collector collector;

    virtual ~parser_holder();

    struct op_data
    {
        std::optional<lexing::u8string_with_newlines> op_text;
        range op_range;
        size_t op_logical_column;
    };

    op_data lab_instr();
    op_data look_lab_instr();

    void op_rem_body_noop();
    void op_rem_body_deferred();
    void lookahead_operands_and_remarks_asm();
    void lookahead_operands_and_remarks_dat();

    semantics::operand_list macro_ops(bool reparse);

    void op_rem_body_ca_expr();
    void op_rem_body_ca_branch();
    void op_rem_body_ca_var_def();

    std::optional<semantics::op_rem> op_rem_body_dat(bool reparse, bool model_allowed);
    std::optional<semantics::op_rem> op_rem_body_mach(bool reparse, bool model_allowed);
    std::optional<semantics::op_rem> op_rem_body_asm(context::id_index opcode, bool reparse, bool model_allowed);

    semantics::operand_ptr ca_op_expr();
    semantics::operand_ptr operand_mach();

    struct mac_op_data
    {
        macop_preprocess_results operands;
        range op_range;
        size_t op_logical_column;
    };

    semantics::literal_si literal_reparse();

    void prepare_parser(lexing::u8string_view_with_newlines text,
        context::hlasm_context* hlasm_ctx,
        diagnostic_op_consumer* diags,
        semantics::range_provider range_prov,
        range text_range,
        size_t logical_column,
        const processing::processing_status& proc_status);

    static std::unique_ptr<parser_holder> create(
        context::hlasm_context* hl_ctx, diagnostic_op_consumer* d, bool multiline);

    struct parser2;

    // testing only
    expressions::ca_expr_ptr testing_expr();
    expressions::data_definition testing_data_def();
};

} // namespace hlasm_plugin::parser_library::parsing

#endif
