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

#include "parser_impl.h"

#include <cctype>
#include <charconv>

#include "context/hlasm_context.h"
#include "context/literal_pool.h"
#include "error_strategy.h"
#include "expressions/conditional_assembly/ca_expr_visitor.h"
#include "expressions/conditional_assembly/ca_expression.h"
#include "expressions/conditional_assembly/terms/ca_constant.h"
#include "hlasmparser_multiline.h"
#include "hlasmparser_singleline.h"
#include "lexing/string_with_newlines.h"
#include "lexing/token_stream.h"
#include "processing/op_code.h"
#include "semantics/operand.h"
#include "utils/string_operations.h"
#include "utils/unicode_text.h"

namespace hlasm_plugin::parser_library::parsing {

parser_impl::parser_impl(antlr4::TokenStream* input)
    : Parser(input)
    , input(dynamic_cast<lexing::token_stream&>(*input))
    , hlasm_ctx(nullptr)
    , provider()
    , err_listener_(&provider)
{
    setBuildParseTree(false);
}

void parser_impl::initialize(context::hlasm_context* hl_ctx, diagnostic_op_consumer* d)
{
    removeErrorListeners();
    addErrorListener(&err_listener_);

    hlasm_ctx = hl_ctx;
    diagnoser_ = d;
    err_listener_.diagnoser = d;
}

void parser_impl::reinitialize(context::hlasm_context* h_ctx,
    semantics::range_provider range_prov,
    processing::processing_status proc_stat,
    diagnostic_op_consumer* d)
{
    hlasm_ctx = h_ctx;
    provider = std::move(range_prov);
    proc_status = proc_stat;
    diagnoser_ = d;
    err_listener_.diagnoser = d;
}

template<bool multiline>
struct parser_holder_impl final : parser_holder
{
    using parser_t = std::conditional_t<multiline, hlasmparser_multiline, hlasmparser_singleline>;
    parser_holder_impl(context::hlasm_context* hl_ctx, diagnostic_op_consumer* d)
    {
        error_handler = std::make_shared<parsing::error_strategy>();
        lex = std::make_unique<lexing::lexer>();
        stream = std::make_unique<lexing::token_stream>(lex.get());
        parser = std::make_unique<parser_t>(stream.get());
        parser->setErrorHandler(error_handler);
        parser->initialize(hl_ctx, d);
    }
    auto& get_parser() const { return static_cast<parser_t&>(*parser); }

    op_data lab_instr() const override
    {
        auto rule = get_parser().lab_instr();
        return { std::move(rule->op_text), rule->op_range, rule->op_logical_column };
    }
    op_data look_lab_instr() const override
    {
        auto rule = get_parser().look_lab_instr();
        return { std::move(rule->op_text), rule->op_range, rule->op_logical_column };
    }

    void op_rem_body_noop() const override { get_parser().op_rem_body_noop(); }
    void op_rem_body_ignored() const override { get_parser().op_rem_body_ignored(); }
    void op_rem_body_deferred() const override { get_parser().op_rem_body_deferred(); }
    void lookahead_operands_and_remarks_asm() const override { get_parser().lookahead_operands_and_remarks_asm(); }
    void lookahead_operands_and_remarks_dat() const override { get_parser().lookahead_operands_and_remarks_dat(); }

    semantics::operand_list macro_ops() const override { return std::move(get_parser().macro_ops()->list); }
    semantics::op_rem op_rem_body_asm_r() const override { return std::move(get_parser().op_rem_body_asm_r()->line); }
    semantics::op_rem op_rem_body_mach_r() const override { return std::move(get_parser().op_rem_body_mach_r()->line); }
    semantics::op_rem op_rem_body_dat_r() const override { return std::move(get_parser().op_rem_body_dat_r()->line); }


    void op_rem_body_ca_expr() const override { get_parser().op_rem_body_ca_expr(); }
    void op_rem_body_ca_branch() const override { get_parser().op_rem_body_ca_branch(); }
    void op_rem_body_ca_var_def() const override { get_parser().op_rem_body_ca_var_def(); }

    void op_rem_body_dat() const override { get_parser().op_rem_body_dat(); }
    void op_rem_body_mach() const override { get_parser().op_rem_body_mach(); }
    void op_rem_body_asm() const override { get_parser().op_rem_body_asm(); }

    operand_ptr ca_op_expr() const override { return std::move(get_parser().ca_op_expr()->op); }
    operand_ptr operand_mach() const override { return std::move(get_parser().operand_mach()->op); }

    semantics::literal_si literal_reparse() const override { return std::move(get_parser().literal_reparse()->value); }
};

std::unique_ptr<parser_holder> parser_holder::create(
    context::hlasm_context* hl_ctx, diagnostic_op_consumer* d, bool multiline)
{
    if (multiline)
        return std::make_unique<parser_holder_impl<true>>(hl_ctx, d);
    else
        return std::make_unique<parser_holder_impl<false>>(hl_ctx, d);
}

void parser_impl::enable_lookahead_recovery()
{
    static_cast<error_strategy*>(_errHandler.get())->enable_lookahead_recovery();
}

void parser_impl::disable_lookahead_recovery()
{
    static_cast<error_strategy*>(_errHandler.get())->disable_lookahead_recovery();
}

void parser_impl::enable_continuation() { input.enable_continuation(); }

void parser_impl::disable_continuation() { input.disable_continuation(); }

bool parser_impl::is_self_def()
{
    std::string tmp(_input->LT(1)->getText());
    utils::to_upper(tmp);
    return tmp == "B" || tmp == "X" || tmp == "C" || tmp == "G";
}

self_def_t parser_impl::parse_self_def_term(const std::string& option, const std::string& value, range term_range)
{
    auto add_diagnostic = diagnoser_ ? diagnostic_adder(*diagnoser_, term_range) : diagnostic_adder(term_range);
    return expressions::ca_constant::self_defining_term(option, value, add_diagnostic);
}

self_def_t parser_impl::parse_self_def_term_in_mach(const std::string& type, const std::string& value, range term_range)
{
    auto add_diagnostic = diagnoser_ ? diagnostic_adder(*diagnoser_, term_range) : diagnostic_adder(term_range);
    if (type.size() == 1)
    {
        switch (type.front())
        {
            case 'b':
            case 'B': {
                if (value.empty())
                    return 0;
                uint32_t res = 0;
                if (auto conv = std::from_chars(value.data(), value.data() + value.size(), res, 2);
                    conv.ec != std::errc() || conv.ptr != value.data() + value.size())
                {
                    add_diagnostic(diagnostic_op::error_CE007);
                    return 0;
                }

                return static_cast<int32_t>(res);
            }
            case 'd':
            case 'D': {
                if (value.empty())
                    return 0;

                auto it = std::ranges::find_if(value, [](auto c) { return c != '-' && c != '+'; });

                if (it - value.begin() > 1 || value.front() == '-' && value.size() > 11)
                {
                    add_diagnostic(diagnostic_op::error_CE007);
                    return 0;
                }

                size_t start = value.front() == '+' ? 1 : 0;

                int32_t res = 0;
                if (auto conv = std::from_chars(value.data() + start, value.data() + value.size(), res, 10);
                    conv.ec != std::errc() || conv.ptr != value.data() + value.size())
                {
                    add_diagnostic(diagnostic_op::error_CE007);
                    return 0;
                }

                return res;
            }
            case 'x':
            case 'X': {
                if (value.empty())
                    return 0;
                uint32_t res = 0;
                if (auto conv = std::from_chars(value.data(), value.data() + value.size(), res, 16);
                    conv.ec != std::errc() || conv.ptr != value.data() + value.size())
                {
                    add_diagnostic(diagnostic_op::error_CE007);
                    return 0;
                }

                return static_cast<int32_t>(res);
            }
            default:
                break;
        }
    }
    return expressions::ca_constant::self_defining_term(type, value, add_diagnostic);
}

context::data_attr_kind parser_impl::get_attribute(std::string attr_data)
{
    // This function is called only from grammar when there are tokens ORDSYMBOL ATTR
    // ATTR is not generated by lexer unless the ordsymbol token has length 1
    auto c = (char)std::toupper((unsigned char)attr_data[0]);
    return context::symbol_attributes::transform_attr(c);
}

context::id_index parser_impl::parse_identifier(std::string value, range id_range)
{
    if (value.size() > 63 && diagnoser_)
        diagnoser_->add_diagnostic(diagnostic_op::error_S100(value, id_range));

    return hlasm_ctx->ids().add(std::move(value));
}

size_t parser_impl::get_loctr_len() const
{
    auto [_, opcode] = *proc_status;
    return processing::processing_status_cache_key::generate_loctr_len(opcode.value.to_string_view());
}

bool parser_impl::loctr_len_allowed(const std::string& attr) const
{
    return (attr == "L" || attr == "l") && proc_status.has_value();
}

void parser_impl::resolve_expression(expressions::ca_expr_ptr& expr, context::SET_t_enum type) const
{
    diagnostic_consumer_transform diags([this](diagnostic_op d) {
        if (diagnoser_)
            diagnoser_->add_diagnostic(std::move(d));
    });
    expr->resolve_expression_tree({ type, type, true }, diags);
}

void parser_impl::resolve_expression(std::vector<expressions::ca_expr_ptr>& expr_list, context::SET_t_enum type) const
{
    for (auto& expr : expr_list)
        resolve_expression(expr, type);
}

void parser_impl::resolve_expression(expressions::ca_expr_ptr& expr) const
{
    diagnostic_consumer_transform diags([this](diagnostic_op d) {
        if (diagnoser_)
            diagnoser_->add_diagnostic(std::move(d));
    });
    auto [_, opcode] = *proc_status;
    using wk = id_storage::well_known;
    if (opcode.value == wk::SETA || opcode.value == wk::ACTR || opcode.value == wk::ASPACE || opcode.value == wk::AGO
        || opcode.value == wk::MHELP)
        resolve_expression(expr, context::SET_t_enum::A_TYPE);
    else if (opcode.value == wk::SETB)
    {
        if (!expr->is_compatible(ca_expression_compatibility::setb))
            diags.add_diagnostic(diagnostic_op::error_CE016_logical_expression_parenthesis(expr->expr_range));

        resolve_expression(expr, context::SET_t_enum::B_TYPE);
    }
    else if (opcode.value == wk::AIF)
    {
        if (!expr->is_compatible(ca_expression_compatibility::aif))
            diags.add_diagnostic(diagnostic_op::error_CE016_logical_expression_parenthesis(expr->expr_range));

        resolve_expression(expr, context::SET_t_enum::B_TYPE);
    }
    else if (opcode.value == wk::SETC)
    {
        resolve_expression(expr, context::SET_t_enum::C_TYPE);
    }
    else if (opcode.value == wk::AREAD)
    {
        // aread operand is just enumeration
    }
    else
    {
        assert(false);
        resolve_expression(expr, context::SET_t_enum::UNDEF_TYPE);
    }
}

void parser_impl::resolve_concat_chain(const semantics::concat_chain& chain) const
{
    diagnostic_consumer_transform diags([this](diagnostic_op d) {
        if (diagnoser_)
            diagnoser_->add_diagnostic(std::move(d));
    });
    for (const auto& e : chain)
        e.resolve(diags);
}

bool parser_impl::ALIAS()
{
    auto& [_, opcode] = *proc_status;
    return opcode.type == instruction_type::ASM && opcode.value == id_storage::well_known::ALIAS;
}

bool parser_impl::END()
{
    auto& [_, opcode] = *proc_status;
    return opcode.type == instruction_type::ASM && opcode.value == id_storage::well_known::END;
}

bool parser_impl::NOT(const antlr4::Token* token) const
{
    if (!token)
        return false;

    static constexpr std::string_view NOT_OPERATOR = "NOT";
    auto token_txt = token->getText();

    return std::ranges::equal(token_txt, NOT_OPERATOR, {}, [](unsigned char c) { return (char)::toupper(c); });
}

bool parser_impl::is_attribute_consuming(char c)
{
    auto tmp = std::toupper(static_cast<int>(c));
    return tmp == 'O' || tmp == 'S' || tmp == 'I' || tmp == 'L' || tmp == 'T';
}

bool parser_impl::is_attribute_consuming(const antlr4::Token* token)
{
    if (!token)
        return false;

    auto text = token->getText();
    return text.size() == 1 && is_attribute_consuming(text.front());
}

bool parser_impl::can_attribute_consume(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '=' || c == '$' || c == '_' || c == '#' || c == '@';
}

bool parser_impl::can_attribute_consume(const antlr4::Token* token)
{
    if (!token)
        return false;

    auto text = token->getText();
    return text.size() > 0 && can_attribute_consume(text.front());
}

antlr4::misc::IntervalSet parser_impl::getExpectedTokens()
{
    if (proc_status && proc_status->first.kind == processing::processing_kind::LOOKAHEAD)
        return {};
    else
        return antlr4::Parser::getExpectedTokens();
}

void parser_impl::add_diagnostic(
    diagnostic_severity severity, std::string code, std::string message, range diag_range) const
{
    add_diagnostic(diagnostic_op(severity, std::move(code), std::move(message), diag_range));
}

void parser_impl::add_diagnostic(diagnostic_op d) const
{
    if (diagnoser_)
        diagnoser_->add_diagnostic(std::move(d));
}

context::id_index parser_impl::add_id(std::string s) const { return hlasm_ctx->ids().add(std::move(s)); }
context::id_index parser_impl::add_id(std::string_view s) const { return hlasm_ctx->ids().add(s); }

void parser_impl::add_label_component(
    const antlr4::Token* token, semantics::concat_chain& chain, std::string& buffer, bool& has_variables) const
{
    auto text = token->getText();
    buffer.append(text);
    if (text == ".")
        chain.emplace_back(dot_conc(provider.get_range(token)));
    else if (text == "=")
        chain.emplace_back(equals_conc(provider.get_range(token)));
    else
        chain.emplace_back(char_str_conc(std::move(text), provider.get_range(token)));
}
void parser_impl::add_label_component(
    semantics::vs_ptr s, semantics::concat_chain& chain, std::string& buffer, bool& has_variables) const
{
    has_variables = true;
    chain.emplace_back(var_sym_conc(std::move(s)));
}

std::string parser_impl::get_context_text(const antlr4::ParserRuleContext* ctx) const
{
    std::string result;
    append_context_text(result, ctx);
    return result;
}

void parser_impl::append_context_text(std::string& s, const antlr4::ParserRuleContext* ctx) const
{
    auto start = ctx->start;
    auto stop = ctx->stop;

    if (!stop)
        stop = input.LT(-1);

    auto startId = start->getTokenIndex();
    auto stopId = stop->getTokenIndex();

    for (auto id = startId; id <= stopId; ++id)
        if (auto token = input.get(id);
            token->getChannel() == lexing::lexer::Channels::DEFAULT_CHANNEL && token->getType() != antlr4::Token::EOF)
            s.append(token->getText());
}

bool parser_impl::goff() const noexcept { return hlasm_ctx->goff(); }

parser_holder::~parser_holder() = default;

void parser_holder::prepare_parser(lexing::u8string_view_with_newlines text,
    context::hlasm_context* hlasm_ctx,
    diagnostic_op_consumer* diags,
    semantics::range_provider range_prov,
    range text_range,
    size_t logical_column,
    const processing::processing_status& proc_status) const
{
    lex->reset(text, text_range.start, logical_column);

    stream->reset();

    parser->reinitialize(hlasm_ctx, std::move(range_prov), proc_status, diags);

    parser->reset();

    parser->get_collector().prepare_for_next_statement();
}

struct parser_holder::macro_preprocessor_t
{
    static constexpr const auto EOF_SYMBOL = lexing::lexer::EOF_SYMBOL;
    const parser_holder* holder;

    using input_state_t = decltype(holder->lex->peek_initial_input_state().first);

    size_t cont;
    input_state_t input;
    const lexing::char_t* data;

    mac_op_data result {};

    void adjust_lines() noexcept
    {
        if (static_cast<size_t>(input.next - data) < *input.nl)
            return;

        input.char_position_in_line = cont;
        input.char_position_in_line_utf16 = cont;
        while (static_cast<size_t>(input.next - data) >= *input.nl)
        {
            ++input.line;
            ++input.nl;
        }
        return;
    }

    void consume() noexcept
    {
        const auto ch = *input.next;
        assert(!eof());

        adjust_lines();
        ++input.next;
        ++input.char_position_in_line;
        input.char_position_in_line_utf16 += 1 + (ch > 0xffffu);
    }

    void copy_char()
    {
        utils::append_utf32_to_utf8(result.operands.text, *input.next);
        consume();
    }

    [[nodiscard]] position cur_pos() noexcept { return position(input.line, input.char_position_in_line_utf16); }

    void consume_rest()
    {
        append_current_operand();
        while (except<U' '>())
            consume();
        adjust_lines();
        if (!eof())
            lex_last_remark();
    }

    [[nodiscard]] auto adjust_range(range r) const noexcept { return holder->parser->provider.adjust_range(r); }

    void add_diagnostic(diagnostic_op (&d)(const range&))
    {
        holder->parser->add_diagnostic(d(holder->parser->provider.adjust_range(range(cur_pos()))));
        holder->error_handler->singal_error();
        consume_rest();
    }

    position last_operand_start;
    void append_current_operand()
    {
        if (const auto e = cur_pos(); last_operand_start != e)
            result.operands.text_ranges.push_back(adjust_range({ last_operand_start, e }));
    }

    void lex_last_remark()
    {
        // skip spaces
        while (follows<U' '>())
            consume();
        adjust_lines();

        const auto last_remark_start = cur_pos();
        while (!eof())
            consume();
        adjust_lines();

        if (const auto last_remark_end = cur_pos(); last_remark_start != last_remark_end)
            result.operands.remarks.push_back(adjust_range({ last_remark_start, last_remark_end }));

        last_operand_start = cur_pos();
    }

    void lex_line_remark()
    {
        while (follows<U' '>() && static_cast<size_t>(input.next - data) < *input.nl)
            consume();

        if (static_cast<size_t>(input.next - data) < *input.nl)
        {
            const auto last_remark_start = cur_pos();
            while (!eof() && static_cast<size_t>(input.next - data) < *input.nl)
                consume();

            if (const auto remark_end = cur_pos(); last_remark_start != remark_end)
                result.operands.remarks.push_back(adjust_range({ last_remark_start, remark_end }));
        }
    }

    static constexpr const auto ord_first =
        utils::create_truth_table("$_#@abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    static constexpr const auto ord =
        utils::create_truth_table("$_#@abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    static constexpr const auto numbers = utils::create_truth_table("0123456789");
    static constexpr const auto identifier_divider = utils::create_truth_table("*.-+=<>,()'/&| ");

    [[nodiscard]] static constexpr bool is_ord_first(char32_t c) noexcept
    {
        return c < ord_first.size() && ord_first[c];
    }
    [[nodiscard]] constexpr bool is_ord_first() const noexcept { return is_ord_first(*input.next); }

    [[nodiscard]] static constexpr bool is_ord(char32_t c) noexcept { return c < ord.size() && ord[c]; }
    [[nodiscard]] constexpr bool is_ord() const noexcept { return is_ord(*input.next); }

    [[nodiscard]] static constexpr bool is_num(char32_t c) noexcept { return c < numbers.size() && numbers[c]; }
    [[nodiscard]] constexpr bool is_num() const noexcept { return is_num(*input.next); }

    [[nodiscard]] constexpr bool eof() const noexcept { return *input.next == EOF_SYMBOL; }

    template<char32_t... chars>
    [[nodiscard]] constexpr bool except() const noexcept requires((chars != EOF_SYMBOL) && ...)
    {
        const auto ch = *input.next;
        return ((ch != EOF_SYMBOL) && ... && (ch != chars));
    }

    template<char32_t... chars>
    [[nodiscard]] constexpr bool follows() const noexcept requires((chars != EOF_SYMBOL) && ...)
    {
        const auto ch = *input.next;
        return ((ch == chars) || ...);
    }

    template<char32_t... chars>
    [[nodiscard]] constexpr bool must_follow(diagnostic_op (&d)(const range&)) requires((chars != EOF_SYMBOL) && ...)
    {
        if (follows<chars...>())
            return true;

        add_diagnostic(d);
        return false;
    }

    static constexpr const auto failure = []() {};

    struct [[nodiscard]] result_t
    {
        bool error = false;

        result_t() = default;
        explicit(false) result_t(decltype(failure))
            : error(true)
        {}
    };

    void copy_ordsymbol()
    {
        assert(is_ord_first());
        do
        {
            copy_char();
        } while (is_ord());
    }

    result_t lex_variable();

    result_t lex_compound_variable()
    {
        assert(follows<U'('>());

        copy_char();

        while (!eof())
        {
            switch (*input.next)
            {
                case U')':
                    copy_char();
                    return {};

                case U'&':
                    if (auto [error] = lex_variable(); error)
                        return failure;
                    break;

                    // TOOD: does not seem right to include these
                    // case U'"':
                    // case U'*':
                    // case U'.':
                    // case U'-':
                    // case U'+':
                    // case U'=':
                    // case U'<':
                    // case U'>':
                    // case U',':
                    // case U'(':
                    // case U'\'':
                    // case U'/':
                    // case U'|':
                    // case U' ':
                    //     return failure;

                default:
                    copy_char();
                    break;
            }
        }
        return {};
    }

    [[nodiscard]] constexpr bool follows_NOT_SPACE() const noexcept
    {
        return (input.next[0] == U'N' || input.next[0] == U'n') && (input.next[1] == U'O' || input.next[1] == U'o')
            && (input.next[2] == U'T' || input.next[2] == U'o') && input.next[3] == U' ';
    }

    result_t lex_expr_general()
    {
        while (follows_NOT_SPACE())
        {
            copy_char();
            copy_char();
            copy_char();
            do
            {
                copy_char();
            } while (follows<U' '>());
        }

        if (auto [error] = lex_expr(); error)
            return failure;

        return {};
    }

    result_t lex_ca_string()
    {
        assert(follows<U'\''>());
        do
        {
            copy_char();

            while (except<U'\''>())
            {
                switch (*input.next)
                {
                    case U'&':
                        if (input.next[1] == U'&')
                        {
                            copy_char();
                            copy_char();
                        }
                        else if (auto [error] = lex_variable(); error)
                            return failure;
                        break;

                    default:
                        copy_char();
                        break;
                }
            }

            if (*input.next != U'\'')
            {
                add_diagnostic(diagnostic_op::error_S0005);
                return failure;
            }

            copy_char();
        } while (follows<U'\''>());

        return {};
    }

    result_t lex_ca_string_with_optional_substring()
    {
        assert(follows<U'\''>());
        if (auto [error] = lex_ca_string(); error)
            return failure;
        if (follows<U'('>())
        {
            copy_char();
            if (auto [error] = lex_expr_general(); error)
                return failure;
            if (*input.next != U',')
            {
                add_diagnostic(diagnostic_op::error_S0003);
                return failure;
            }
            copy_char();

            if (follows<U'*'>())
                copy_char();
            else if (eof())
            {
                add_diagnostic(diagnostic_op::error_S0003);
                return failure;
            }
            else if (auto [error] = lex_expr_general(); error)
                return failure;

            if (*input.next != U')')
            {
                add_diagnostic(diagnostic_op::error_S0011);
                return failure;
            }
            copy_char();
        }
        return {};
    }

    void lex_optional_space()
    {
        while (follows<U' '>())
            copy_char();
    }

    result_t lex_subscript_ne()
    {
        assert(follows<U'('>());

        copy_char();
        if (follows<U' '>())
        {
            lex_optional_space();
            if (auto [error] = lex_expr(); error)
                return failure;
            lex_optional_space();
        }
        else
        {
            if (auto [error] = lex_expr(); error)
                return failure;
            if (*input.next != U',')
            {
                add_diagnostic(diagnostic_op::error_S0002);
                return failure;
            }
            copy_char();
            if (eof())
            {
                add_diagnostic(diagnostic_op::error_S0003);
                return failure;
            }
            if (auto [error] = lex_expr(); error)
                return failure;
            while (follows<U','>())
            {
                copy_char();
                if (eof())
                {
                    add_diagnostic(diagnostic_op::error_S0003);
                    return failure;
                }
                if (auto [error] = lex_expr(); error)
                    return failure;
            }
        }
        if (!must_follow<U')'>(diagnostic_op::error_S0011))
            return failure;
        copy_char();

        return {};
    }

    result_t lex_term()
    {
        switch (*input.next)
        {
            case EOF_SYMBOL:
                add_diagnostic(diagnostic_op::error_S0003);
                return failure;

            case U'&':
                if (auto [error] = lex_variable(); error)
                    return failure;
                return {};

            case U'-':
                copy_char();
                if (!follows<U'0', U'1', U'2', U'3', U'4', U'5', U'6', U'7', U'8', U'9'>())
                {
                    add_diagnostic(diagnostic_op::error_S0002);
                    return failure;
                }
                [[fallthrough]];
            case U'0':
            case U'1':
            case U'2':
            case U'3':
            case U'4':
            case U'5':
            case U'6':
            case U'7':
            case U'8':
            case U'9':
                do
                {
                    copy_char();
                } while (is_num());
                return {};

            case U'\'':
                if (auto [error] = lex_ca_string_with_optional_substring(); error)
                    return failure;
                return {};

            case U'(': {
                copy_char();
                if (eof())
                {
                    add_diagnostic(diagnostic_op::error_S0003);
                    return failure;
                }

                if (!follows_NOT_SPACE())
                {
                    auto spaces_found = follows<U' '>();
                    size_t exprs = 0;
                    while (except<U')'>())
                    {
                        spaces_found |= follows<U' '>();
                        lex_optional_space();
                        if (auto [error] = lex_expr(); error)
                            return failure;
                        ++exprs;
                    }
                    spaces_found |= follows<U' '>();
                    lex_optional_space();
                    if (!must_follow<U')'>(diagnostic_op::error_S0011))
                        return failure;
                    copy_char();
                    if (spaces_found || exprs > 0)
                        return {};
                }
                else if (auto [error] = lex_expr_general(); error)
                    return failure;
                else if (!must_follow<U')'>(diagnostic_op::error_S0011))
                    return failure;
                else
                    copy_char();

                if (follows<U'\''>())
                {
                    if (auto [error] = lex_ca_string_with_optional_substring(); error)
                        return failure;
                    while (follows<U'(', U'\''>())
                    {
                        if (follows<U'('>())
                        {
                            copy_char();
                            if (auto [error] = lex_expr_general(); error)
                                return failure;
                            if (!must_follow<U')'>(diagnostic_op::error_S0011))
                                return failure;
                            copy_char();
                        }
                        if (auto [error] = lex_ca_string_with_optional_substring(); error)
                            return failure;
                    }
                }
                else if (is_ord_first())
                {
                    copy_ordsymbol();
                    if (!must_follow<U'('>(diagnostic_op::error_S0002))
                        return failure;
                    if (auto [error] = lex_subscript_ne(); error)
                        return failure;
                }
                return {};
            }

            default:
                if (!is_ord_first())
                {
                    add_diagnostic(diagnostic_op::error_S0002);
                    return failure;
                }
                if (input.next[1] == U'\'')
                {
                    switch (*input.next)
                    {
                        case U'B':
                        case U'X':
                        case U'C':
                        case U'G':
                        case U'b':
                        case U'x':
                        case U'c':
                        case U'g':
                            copy_char();
                            while (follows<U'\''>())
                            {
                                if (auto [error] = lex_simple_string(); error)
                                    return failure;
                            }
                            return {};

                        case U'N':
                        case U'K':
                        case U'D':
                        case U'O':
                        case U'S':
                        case U'I':
                        case U'L':
                        case U'T':
                        case u'n':
                        case u'k':
                        case u'd':
                        case U'o':
                        case U's':
                        case U'i':
                        case U'l':
                        case U't':
                            copy_char();
                            copy_char();
                            switch (*input.next)
                            {
                                case EOF_SYMBOL:
                                    add_diagnostic(diagnostic_op::error_S0003);
                                    return failure;
                                case U'&':
                                    if (auto [error] = lex_variable(); error)
                                        return failure;
                                    break;
                                case U'=':
                                    if (auto [error] = lex_literal(); error)
                                        return failure;
                                    break;
                                default:
                                    if (!is_ord_first())
                                    {
                                        add_diagnostic(diagnostic_op::error_S0002);
                                        return failure;
                                    }
                                    copy_ordsymbol();
                                    break;
                            }
                            return {};
                    }
                }
                copy_ordsymbol();
                if (follows<U'('>())
                {
                    if (auto [error] = lex_subscript_ne(); error)
                        return failure;
                }
                return {};
        }
    }

    result_t lex_mach_term()
    {
        switch (*input.next)
        {
            case EOF_SYMBOL:
                add_diagnostic(diagnostic_op::error_S0003);
                return failure;

            case U'(':
                copy_char();
                if (auto [error] = lex_mach_expr(); error)
                    return failure;
                if (!must_follow<U')'>(diagnostic_op::error_S0011))
                    return failure;
                copy_char();
                break;

            case U'*':
                copy_char();
                break;

            case U'-':
                copy_char();
                if (!is_num())
                {
                    add_diagnostic(diagnostic_op::error_S0002);
                    return failure;
                }
                [[fallthrough]];
            case U'0':
            case U'1':
            case U'2':
            case U'3':
            case U'4':
            case U'5':
            case U'6':
            case U'7':
            case U'8':
            case U'9':
                do
                {
                    copy_char();
                } while (is_num());
                break;

            case U'=':
                if (auto [error] = lex_literal(); error)
                    return failure;
                break;

            default:
                if (!is_ord_first())
                {
                    add_diagnostic(diagnostic_op::error_S0002);
                    return failure;
                }
                if (follows<U'C', U'c'>() && (input.next[1] == U'A' || input.next[1] == U'a') && input.next[2] == U'\'')
                {
                    copy_char();
                    copy_char();
                    if (auto [error] = lex_mach_string(); error)
                        return failure;
                    return {};
                }
                if (input.next[1] == U'\'')
                {
                    switch (*input.next)
                    {
                        case U'L':
                        case U'l':
                            copy_char();
                            copy_char();
                            if (follows<U'*'>())
                            {
                                copy_char();
                                return {};
                            }
                            break;

                        case U'O':
                        case U'S':
                        case U'I':
                        case U'T':
                        case U'o':
                        case U's':
                        case U'i':
                        case U't':
                            copy_char();
                            copy_char();
                            break;

                        case U'B':
                        case U'D':
                        case U'X':
                        case U'C':
                        case U'b':
                        case U'd':
                        case U'x':
                        case U'c':
                            copy_char();
                            if (auto [error] = lex_mach_string(); error)
                                return failure;
                            return {};
                    }
                }
                if (!is_ord_first())
                {
                    add_diagnostic(diagnostic_op::error_S0002);
                    return failure;
                }
                copy_ordsymbol();
                break;
        }
        return {};
    }

    result_t lex_mach_string()
    {
        assert(follows<U'\''>());

        copy_char();

        while (!eof())
        {
            if (input.next[0] != U'\'')
            {
                copy_char();
            }
            else if (input.next[1] == U'\'')
            {
                copy_char();
                copy_char();
            }
            else
            {
                copy_char();
                return {};
            }
        }

        add_diagnostic(diagnostic_op::error_S0005);
        return failure;
    }

    result_t lex_mach_term_c()
    {
        while (follows<U'+'>() || (follows<U'-'>() && !is_num(input.next[1])))
            copy_char();

        if (auto [error] = lex_mach_term(); error)
            return failure;

        return {};
    }

    result_t lex_mach_expr_s()
    {
        if (auto [error] = lex_mach_term_c(); error)
            return failure;

        while (follows<U'*', U'/'>())
        {
            copy_char();
            if (auto [error] = lex_mach_term_c(); error)
                return failure;
        }

        return {};
    }

    result_t lex_mach_expr()
    {
        if (auto [error] = lex_mach_expr_s(); error)
            return failure;

        while (follows<U'+', U'-'>())
        {
            copy_char();
            if (auto [error] = lex_mach_expr_s(); error)
                return failure;
        }

        return {};
    }

    static bool is_type_extension(char type, char ch)
    {
        return checking::data_def_type::types_and_extensions.count(std::make_pair(type, ch)) > 0;
    }

    result_t lex_literal_signed_num()
    {
        if (follows<U'('>())
        {
            copy_char();
            if (auto [error] = lex_mach_expr(); error)
                return failure;
            if (!must_follow<U')'>(diagnostic_op::error_S0011))
                return failure;
            copy_char();
        }
        else
        {
            if (follows<U'+', U'-'>())
                copy_char();
            if (!is_num())
            {
                add_diagnostic(diagnostic_op::error_S0002);
                return failure;
            }
            do
            {
                copy_char();
            } while (is_num());
        }
        return {};
    }

    result_t lex_literal_unsigned_num()
    {
        if (follows<U'('>())
        {
            copy_char();
            if (auto [error] = lex_mach_expr(); error)
                return failure;
            if (!must_follow<U')'>(diagnostic_op::error_S0011))
                return failure;
            copy_char();
        }
        else if (is_num())
        {
            do
            {
                copy_char();
            } while (is_num());
        }
        else
        {
            add_diagnostic(diagnostic_op::error_S0002);
            return failure;
        }
        return {};
    }

    result_t lex_data_def_base()
    {
        // case state::duplicating_factor:
        if (follows<U'('>() || is_num())
        {
            if (auto [error] = lex_literal_unsigned_num(); error)
                return failure;
        }

        // case state::read_type:
        if (!is_ord_first())
        {
            add_diagnostic(diagnostic_op::error_S0002);
            return failure;
        }
        const auto type = *input.next;
        copy_char();

        if (is_ord_first() && is_type_extension((char)type, (char)*input.next))
        {
            copy_char();
        }

        // case state::try_reading_program:
        // case state::read_program:
        if (const auto c = *input.next; c == U'P' || c == U'p')
        {
            copy_char();
            if (auto [error] = lex_literal_signed_num(); error)
                return failure;
        }
        // case state::try_reading_length:
        // case state::try_reading_bitfield:
        // case state::read_length:
        if (const auto c = *input.next; c == U'L' || c == U'l')
        {
            copy_char();
            if (auto [error] = lex_literal_unsigned_num(); error)
                return failure;
        }

        // case state::try_reading_scale:
        // case state::read_scale:
        if (const auto c = *input.next; c == U'S' || c == U's')
        {
            copy_char();
            if (auto [error] = lex_literal_signed_num(); error)
                return failure;
        }

        // case state::try_reading_exponent:
        // case state::read_exponent:
        if (const auto c = *input.next; c == U'E' || c == U'e')
        {
            copy_char();
            if (auto [error] = lex_literal_signed_num(); error)
                return failure;
        }
        return {};
    }

    result_t lex_expr_or_addr()
    {
        if (auto [error] = lex_mach_expr(); error)
            return failure;
        if (follows<U'('>())
        {
            copy_char();
            if (auto [error] = lex_mach_expr(); error)
                return failure;
            if (!must_follow<U')'>(diagnostic_op::error_S0011))
                return failure;
            copy_char();
        }
        return {};
    }


    result_t lex_literal_nominal_char()
    {
        assert(follows<U'\''>());
        copy_char();
        while (true)
        {
            if (input.next[0] == U'\'' && input.next[0] == U'\'')
            {
                copy_char();
                copy_char();
            }
            else if (except<U'\''>())
                copy_char();
            else
                break;
        }
        if (!must_follow<U'\''>(diagnostic_op::error_S0005))
            return failure;
        copy_char();

        return {};
    }


    result_t lex_literal_nominal_addr()
    {
        assert(follows<U'('>());
        copy_char();

        if (auto [error] = lex_expr_or_addr(); error)
            return failure;

        while (follows<U','>())
        {
            copy_char();
            if (auto [error] = lex_expr_or_addr(); error)
                return failure;
        }

        if (!must_follow<U')'>(diagnostic_op::error_S0011))
            return failure;
        copy_char();

        return {};
    }

    result_t lex_literal_nominal()
    {
        if (follows<U'\''>())
        {
            if (auto [error] = lex_literal_nominal_char(); error)
                return failure;
        }
        else if (follows<U'('>())
        {
            if (auto [error] = lex_literal_nominal_addr(); error)
                return failure;
        }
        else
        {
            add_diagnostic(diagnostic_op::error_S0003);
            return failure;
        }
        return {};
    }

    result_t lex_literal()
    {
        assert(follows<U'='>());
        copy_char();

        if (auto [error] = lex_data_def_base(); error)
            return failure;

        if (auto [error] = lex_literal_nominal(); error)
            return failure;

        return {};
    }

    result_t lex_simple_string()
    {
        assert(follows<U'\''>());

        copy_char();
        while (except<U'\'', U'&'>())
            copy_char();
        if (*input.next != U'\'')
        {
            add_diagnostic(diagnostic_op::error_S0005);
            return failure;
        }
        copy_char();

        return {};
    }

    result_t lex_term_c()
    {
        while (input.next[0] == U'+' || (input.next[0] == U'-' && !is_num(input.next[1])))
            copy_char();
        if (auto [error] = lex_term(); error)
            return failure;
        return {};
    }

    result_t lex_expr_s()
    {
        if (auto [error] = lex_term_c(); error)
            return failure;

        while (follows<U'*', U'/'>())
        {
            copy_char();
            if (auto [error] = lex_term_c(); error)
                return failure;
        }

        return {};
    }

    result_t lex_expr()
    {
        if (auto [error] = lex_expr_s(); error)
            return failure;

        switch (*input.next)
        {
            case '+':
            case '-':
                while (follows<U'+', U'-'>())
                {
                    copy_char();
                    if (auto [error] = lex_expr_s(); error)
                        return failure;
                }
                break;
            case '.':
                while (follows<U'.'>())
                {
                    copy_char();
                    if (auto [error] = lex_term_c(); error)
                        return failure;
                }
                break;
        }

        return {};
    }

    result_t lex_subscript()
    {
        assert(follows<U'('>());
        copy_char();

        while (!eof())
        {
            switch (*input.next)
            {
                case U')':
                    copy_char();
                    return {};

                case U',':
                    copy_char();
                    break;

                default:
                    if (auto [error] = lex_expr(); error)
                        return failure;
                    break;
            }
        }

        add_diagnostic(diagnostic_op::error_S0011);
        return failure;
    }

    result_t lex_operand()
    {
        bool next_char_special = true;
        while (!eof())
        {
            const bool last_char_special = std::exchange(next_char_special, true);
            switch (*input.next)
            {
                case U'(':
                    if (auto [error] = process_list(); error)
                        return failure;
                    break;

                case U' ':
                case U')':
                case U',':
                    return {};

                case U'\'':
                    copy_char();
                    while (except<U'\''>())
                    {
                        if (except<U'&'>())
                            copy_char();
                        else if (input.next[1] == U'&')
                        {
                            copy_char();
                            copy_char();
                        }
                        else if (auto [error] = lex_variable(); error)
                            return failure;
                    }
                    if (*input.next != U'\'') // also means EOF
                    {
                        add_diagnostic(diagnostic_op::error_S0005);
                        return failure;
                    }
                    copy_char();
                    next_char_special = false;
                    break;

                case U'&':
                    if (input.next[1] == U'&')
                    {
                        copy_char();
                        copy_char();
                    }
                    else if (auto [error] = lex_variable(); error)
                    {
                        return failure;
                    }
                    next_char_special = false;
                    break;

                case U'O':
                case U'S':
                case U'I':
                case U'L':
                case U'T':
                case U'o':
                case U's':
                case U'i':
                case U'l':
                case U't':
                    if (!last_char_special || input.next[1] != U'\'')
                    {
                        copy_char();
                        next_char_special = false;
                        break;
                    }
                    else if (is_ord_first(input.next[2]) || input.next[2] == '=')
                    {
                        copy_char();
                        copy_char();
                        next_char_special = false;
                        break;
                    }
                    else if (input.next[2] != U'&' && input.next[2] != EOF_SYMBOL)
                    {
                        copy_char();
                        next_char_special = false;
                        break;
                    }

                    while (except<U',', U')', U' '>())
                    {
                        if (*input.next != U'&')
                            copy_char();
                        else if (input.next[1] == U'&')
                        {
                            copy_char();
                            copy_char();
                        }
                        else if (auto [error] = lex_variable(); error)
                            return failure;
                    }
                    break;

                default:
                    next_char_special = *input.next >= ord.size() || !ord[*input.next];
                    copy_char();
                    break;
            }
        }
        return {};
    }

    void process_optional_line_remark()
    {
        if (follows<U' '>() && static_cast<size_t>(input.next - data) < *input.nl)
        {
            append_current_operand();

            lex_line_remark();

            adjust_lines();
            last_operand_start = cur_pos();
        }
    }

    result_t process_list()
    {
        assert(follows<U'('>());

        copy_char();

        if (auto [error] = lex_operand(); error)
            return failure;

        while (follows<U','>())
        {
            copy_char();
            process_optional_line_remark();
            if (auto [error] = lex_operand(); error)
                return failure;
        }

        if (*input.next != U')')
        {
            add_diagnostic(diagnostic_op::error_S0011);
            return failure;
        }
        copy_char();

        return {};
    }

    void preprocess(bool reparse)
    {
        result.op_range = adjust_range({ cur_pos(), cur_pos() });
        result.op_logical_column = input.char_position_in_line;
        result.operands.total_op_range = result.op_range;

        if (eof())
            return;

        if (!reparse && *input.next != U' ')
        {
            add_diagnostic(diagnostic_op::error_S0002);
            consume_rest();
            result.op_range.end = cur_pos();
            return;
        }

        // skip spaces
        while (follows<U' '>())
            consume();
        adjust_lines();

        result.op_range = range(cur_pos());
        result.op_logical_column = input.char_position_in_line;

        last_operand_start = cur_pos();

        // process operands
        while (!eof())
        {
            switch (*input.next)
            {
                case U' ':
                    append_current_operand();
                    lex_last_remark();
                    goto end;

                case U',':
                    copy_char();
                    process_optional_line_remark();
                    break;

                case U'O':
                case U'S':
                case U'I':
                case U'L':
                case U'T':
                case U'o':
                case U's':
                case U'i':
                case U'l':
                case U't':
                    if (input.next[1] == U'\'')
                    {
                        if (auto [error] = lex_operand(); error)
                        {
                            consume_rest();
                            goto end;
                        }
                        break;
                    }
                    [[fallthrough]];

                case U'$':
                case U'_':
                case U'#':
                case U'@':
                case U'a':
                case U'b':
                case U'c':
                case U'd':
                case U'e':
                case U'f':
                case U'g':
                case U'h':
                case U'j':
                case U'k':
                case U'm':
                case U'n':
                case U'p':
                case U'q':
                case U'r':
                case U'u':
                case U'v':
                case U'w':
                case U'x':
                case U'y':
                case U'z':
                case U'A':
                case U'B':
                case U'C':
                case U'D':
                case U'E':
                case U'F':
                case U'G':
                case U'H':
                case U'J':
                case U'K':
                case U'M':
                case U'N':
                case U'P':
                case U'Q':
                case U'R':
                case U'U':
                case U'V':
                case U'W':
                case U'X':
                case U'Y':
                case U'Z':
                    copy_ordsymbol();
                    if (follows<U'='>())
                        copy_char();
                    if (const auto n = *input.next; n == EOF_SYMBOL || n == U' ' || n == U',')
                        continue;
                    [[fallthrough]];

                case U'\'':
                case U'&':
                default:
                    if (auto [error] = lex_operand(); error)
                    {
                        consume_rest();
                        goto end;
                    }
                    break;

                case U')':
                    add_diagnostic(diagnostic_op::error_S0012);
                    consume_rest();
                    goto end;

                case U'(':
                    if (auto [error] = process_list(); error)
                    {
                        consume_rest();
                        goto end;
                    }
                    break;
            }
        }
    end:;
        append_current_operand();

        if (auto& ops = result.operands; !ops.text_ranges.empty())
            ops.total_op_range = union_range(ops.text_ranges.front(), ops.text_ranges.back());

        adjust_lines();
        result.op_range.end = cur_pos();
    }

    macro_preprocessor_t(const parser_holder* h)
        : holder(h)
        , cont(h->lex->get_continuation_column())
    {
        std::tie(input, data) = holder->lex->peek_initial_input_state();
    }
};

parser_holder::macro_preprocessor_t::result_t parser_holder::macro_preprocessor_t::lex_variable()
{
    assert(follows<U'&'>());
    copy_char();
    if (follows<U'('>())
    {
        if (auto [error] = lex_compound_variable(); error)
            return failure;
    }
    else if (!is_ord_first())
    {
        add_diagnostic(diagnostic_op::error_S0008);
        return failure;
    }
    else
        copy_ordsymbol();

    if (follows<U'('>())
    {
        if (auto [error] = lex_subscript(); error)
            return failure;
    }

    return {};
}


parser_holder::mac_op_data parser_holder::macro_preprocessor(bool reparse) const
{
    macro_preprocessor_t p(this);
    p.preprocess(reparse);
    return std::move(p.result);
}

} // namespace hlasm_plugin::parser_library::parsing
