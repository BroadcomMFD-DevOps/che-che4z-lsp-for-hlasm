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

#include "Parser.h"
#include "parser_error_listener.h"
#include "semantics/collector.h"

namespace hlasm_plugin::parser_library::context {
class hlasm_context;
} // namespace hlasm_plugin::parser_library::context

namespace hlasm_plugin::parser_library::lexing {
class lexer;
class token_stream;
struct u8string_view_with_newlines;
} // namespace hlasm_plugin::parser_library::lexing

namespace hlasm_plugin::parser_library::parsing {

using self_def_t = std::int32_t;

class error_strategy;

struct macop_preprocess_results
{
    std::string text;
    std::vector<range> text_ranges;
    range total_op_range;
    std::vector<range> remarks;
};

struct parser_holder;

// class providing methods helpful for parsing and methods modifying parsing process
class parser_impl : public antlr4::Parser
{
    friend struct parser_holder;

public:
    parser_impl(antlr4::TokenStream* input);

    void initialize(
        context::hlasm_context* hlasm_ctx, diagnostic_op_consumer* diagnoser, semantics::collector& collector);

    void reinitialize(context::hlasm_context* hlasm_ctx,
        semantics::range_provider range_prov,
        processing::processing_status proc_stat,
        diagnostic_op_consumer* diagnoser);

    void set_diagnoser(diagnostic_op_consumer* diagnoser)
    {
        diagnoser_ = diagnoser;
        err_listener_.diagnoser = diagnoser;
    }

    static bool is_attribute_consuming(char c);
    static bool is_attribute_consuming(const antlr4::Token* token);

    static bool can_attribute_consume(char c);
    static bool can_attribute_consume(const antlr4::Token* token);

protected:
    void enable_lookahead_recovery();
    void disable_lookahead_recovery();
    void enable_continuation();
    void disable_continuation();
    bool is_self_def();

    context::data_attr_kind get_attribute(std::string attr_data);
    int get_loctr_len() const;
    std::optional<int> maybe_loctr_len() const;
    bool loctr_len_allowed(const std::string& attr) const;

    lexing::token_stream& input;
    context::hlasm_context* hlasm_ctx = nullptr;
    std::optional<processing::processing_status> proc_status;
    semantics::range_provider provider;
    semantics::collector* collector = nullptr;

    bool ALIAS();
    bool END();
    bool NOT(const antlr4::Token* token) const;

    context::id_index add_id(std::string s) const;
    context::id_index add_id(std::string_view s) const;

    std::string get_context_text(const antlr4::ParserRuleContext* ctx) const;
    void append_context_text(std::string& s, const antlr4::ParserRuleContext* ctx) const;

    bool goff() const noexcept;

private:
    antlr4::misc::IntervalSet getExpectedTokens() override;
    diagnostic_op_consumer* diagnoser_ = nullptr;
    parser_error_listener err_listener_;

    bool ca_string_enabled = true;
    bool literals_allowed = true;
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
