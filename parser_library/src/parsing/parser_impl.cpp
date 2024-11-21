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
#include <concepts>
#include <utility>

#include "context/hlasm_context.h"
#include "context/literal_pool.h"
#include "error_strategy.h"
#include "expressions/conditional_assembly/ca_expr_visitor.h"
#include "expressions/conditional_assembly/ca_expression.h"
#include "expressions/conditional_assembly/terms/ca_constant.h"
#include "expressions/nominal_value.h"
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

self_def_t parser_impl::parse_self_def_term(std::string_view option, std::string_view value, range term_range)
{
    auto add_diagnostic = diagnoser_ ? diagnostic_adder(*diagnoser_, term_range) : diagnostic_adder(term_range);
    return expressions::ca_constant::self_defining_term(option, value, add_diagnostic);
}

self_def_t parser_impl::parse_self_def_term_in_mach(std::string_view type, std::string_view value, range term_range)
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

                if (it - value.begin() > 1 || (value.front() == '-' && value.size() > 11))
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

int parser_impl::get_loctr_len() const
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
    return c == 'O' || c == 'S' || c == 'I' || c == 'L' || c == 'T' || c == 'o' || c == 's' || c == 'i' || c == 'l'
        || c == 't';
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

    std::vector<range> remarks;
    mac_op_data mac_op_results {};

    [[nodiscard]] constexpr bool before_nl() const noexcept
    {
        return static_cast<size_t>(input.next - data) < *input.nl;
    }

    void adjust_lines() noexcept
    {
        if (before_nl())
            return;

        input.char_position_in_line = cont;
        input.char_position_in_line_utf16 = cont;
        while (!before_nl())
        {
            ++input.line;
            ++input.nl;
        }
        return;
    }

    template<hl_scopes... s>
    requires(sizeof...(s) <= 1) void consume() noexcept
    {
        const auto ch = *input.next;
        assert(!eof());

        [[maybe_unused]] const auto pos = cur_pos_adjusted();

        ++input.next;
        ++input.char_position_in_line;
        input.char_position_in_line_utf16 += 1 + (ch > 0xffffu);

        [[maybe_unused]] const auto end = cur_pos();

        if constexpr (sizeof...(s))
        {
            add_hl_symbol<s...>({ pos, end });
        }
    }

    template<size_t len = 1>
    requires(len > 0) void consume_into(std::string& s)
    {
        assert(!eof());
        utils::append_utf32_to_utf8(s, *input.next);
        consume();
        if constexpr (len > 1)
            consume_into<len - 1>(s);
    }

    [[nodiscard]] position cur_pos() noexcept { return position(input.line, input.char_position_in_line_utf16); }
    [[nodiscard]] position cur_pos_adjusted() noexcept
    {
        adjust_lines();
        return cur_pos();
    }

    void consume_rest()
    {
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

    void add_diagnostic_or_eof(diagnostic_op (&d)(const range&))
    {
        if (*input.next == EOF_SYMBOL)
            add_diagnostic(diagnostic_op::error_S0003);
        else
            add_diagnostic(d);
    }

    void add_diagnostic(diagnostic_op d)
    {
        holder->parser->add_diagnostic(std::move(d));
        holder->error_handler->singal_error();
        consume_rest();
    }

    template<hl_scopes s>
    void add_hl_symbol(const range& r)
    {
        add_hl_symbol_adjusted<s>(adjust_range(r));
    }

    template<hl_scopes s>
    void add_hl_symbol_adjusted(const range& r)
    {
        holder->parser->get_collector().add_hl_symbol(token_info(r, s));
    }

    void lex_last_remark()
    {
        // skip spaces
        while (follows<U' '>())
            consume();

        const auto last_remark_start = cur_pos_adjusted();
        while (!eof())
            consume();
        adjust_lines();

        if (const auto last_remark_end = cur_pos(); last_remark_start != last_remark_end)
            remarks.push_back(adjust_range({ last_remark_start, last_remark_end }));
    }

    void lex_line_remark()
    {
        assert(follows<U' '>() && before_nl());

        while (follows<U' '>() && before_nl())
            consume();

        if (before_nl())
        {
            const auto last_remark_start = cur_pos(); // adjusted by construction
            while (!eof() && before_nl())
                consume();

            if (const auto remark_end = cur_pos(); last_remark_start != remark_end)
                remarks.push_back(adjust_range({ last_remark_start, remark_end }));
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
    [[nodiscard]] constexpr bool must_follow() requires((chars != EOF_SYMBOL) && ...)
    {
        if (follows<chars...>())
            return true;

        if (*input.next == EOF_SYMBOL)
            add_diagnostic(diagnostic_op::error_S0003);
        else
            add_diagnostic(diagnostic_op::error_S0002);

        return false;
    }

    template<char32_t... chars>
    [[nodiscard]] constexpr bool must_follow(diagnostic_op (&d)(const range&)) requires((chars != EOF_SYMBOL) && ...)
    {
        if (follows<chars...>())
            return true;

        add_diagnostic(d);
        return false;
    }

    template<char32_t... chars>
    [[nodiscard]] constexpr bool match(diagnostic_op (&d)(const range&)) requires((chars != EOF_SYMBOL) && ...)
    {
        if (!follows<chars...>())
        {
            add_diagnostic(d);
            return false;
        }
        consume();
        return true;
    }

    template<hl_scopes s, char32_t... chars>
    [[nodiscard]] constexpr bool match(diagnostic_op (&d)(const range&)) requires((chars != EOF_SYMBOL) && ...)
    {
        if (!follows<chars...>())
        {
            add_diagnostic(d);
            return false;
        }
        consume<s>();
        return true;
    }

    template<char32_t... chars>
    [[nodiscard]] constexpr bool match() requires((chars != EOF_SYMBOL) && ...)
    {
        if (must_follow<chars...>())
        {
            consume();
            return true;
        }
        return false;
    }

    template<hl_scopes s, char32_t... chars>
    [[nodiscard]] constexpr bool match() requires((chars != EOF_SYMBOL) && ...)
    {
        if (must_follow<chars...>())
        {
            consume<s>();
            return true;
        }
        return false;
    }

    struct
    {
    } static constexpr const failure = {};

    template<typename T = void>
    struct [[nodiscard]] result_t
    {
        bool error = false;
        T value = T {};

        template<typename... Args>
        constexpr result_t(Args&&... args) requires(sizeof...(Args) > 0 && std::constructible_from<T, Args...>)
            : value(std::forward<Args>(args)...)
        {}
        constexpr explicit(false) result_t(decltype(failure))
            : error(true)
        {}
    };

    struct [[nodiscard]] result_t_void
    {
        bool error = false;

        constexpr result_t_void() = default;
        constexpr explicit(false) result_t_void(decltype(failure))
            : error(true)
        {}
    };

    result_t<id_index> lex_id()
    {
        assert(is_ord_first());

        std::string name;

        const auto start = cur_pos_adjusted();
        do
        {
            consume_into(name);
        } while (is_ord());
        const auto end = cur_pos();

        auto id = holder->parser->parse_identifier(std::move(name), { start, end });
        if (id.empty())
            return failure;
        else
            return id;
    }

    result_t<std::pair<id_index, id_index>> lex_qualified_id()
    {
        auto [error, id1] = lex_id();
        if (error)
            return failure;

        if (follows<U'.'>())
        {
            consume<hl_scopes::operator_symbol>();
            if (!is_ord_first())
            {
                add_diagnostic(diagnostic_op::error_S0002);
                return failure;
            }

            auto [error2, id2] = lex_id();
            if (error2)
                return failure;

            return { id1, id2 };
        }

        return { id_index(), id1 };
    }

    result_t<vs_ptr> lex_variable();

    result_t<concat_chain> lex_compound_variable()
    {
        if (!except<U')'>())
        {
            add_diagnostic(diagnostic_op::error_S0002);
            return failure;
        }
        concat_chain result;

        while (!eof())
        {
            switch (*input.next)
            {
                case U')':
                    return result;

                case U'&': {
                    auto [error, var] = lex_variable();
                    if (error)
                        return failure;
                    result.emplace_back(std::in_place_type<var_sym_conc>, std::move(var));
                    break;
                }

                    // TODO: does not seem right to include these
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
                case U'.': {
                    const auto start = cur_pos_adjusted();
                    consume<hl_scopes::operator_symbol>();
                    result.emplace_back(std::in_place_type<dot_conc>, adjust_range({ start, cur_pos() }));
                    break;
                }

                default: {
                    const auto start = cur_pos_adjusted();
                    std::string collected;

                    while (except<U')', U'&', U'.'>())
                    {
                        consume_into(collected);
                    }
                    const auto r = adjust_range({ start, cur_pos() });
                    result.emplace_back(std::in_place_type<char_str_conc>, std::move(collected), r);
                    add_hl_symbol_adjusted<hl_scopes::var_symbol>(r);
                    break;
                }
            }
        }
        add_diagnostic(diagnostic_op::error_S0003);
        return failure;
    }

    [[nodiscard]] constexpr bool follows_NOT_SPACE() const noexcept
    {
        return (input.next[0] == U'N' || input.next[0] == U'n') && (input.next[1] == U'O' || input.next[1] == U'o')
            && (input.next[2] == U'T' || input.next[2] == U'o') && input.next[3] == U' ';
    }

    result_t<ca_expr_ptr> lex_expr_general()
    {
        const auto start = cur_pos_adjusted();
        if (!follows_NOT_SPACE())
            return lex_expr();

        std::vector<ca_expr_ptr> ca_exprs;
        do
        {
            const auto start_not = cur_pos_adjusted();
            consume();
            consume();
            consume();
            const auto r = adjust_range({ start_not, cur_pos() });
            add_hl_symbol_adjusted<hl_scopes::operand>(r);
            ca_exprs.push_back(std::make_unique<ca_symbol>(id_index("NOT"), r));
            lex_optional_space();
        } while (follows_NOT_SPACE());

        auto [error, e] = lex_expr();
        if (error)
            return failure;

        ca_exprs.push_back(std::move(e));
        return std::make_unique<ca_expr_list>(std::move(ca_exprs), adjust_range({ start, cur_pos() }), false);
    }

    result_t<concat_chain> lex_ca_string_value()
    {
        assert(follows<U'\''>());
        consume<hl_scopes::operator_symbol>();

        concat_chain cc;

        auto start = cur_pos_adjusted();
        std::string s;
        const auto dump_s = [&]() {
            if (s.empty())
                return;
            cc.emplace_back(std::in_place_type<char_str_conc>, std::move(s), adjust_range({ start, cur_pos() }));
            s.clear();
        };
        while (!eof())
        {
            switch (*input.next)
            {
                case U'.':
                    dump_s();
                    start = cur_pos_adjusted();
                    consume();
                    cc.emplace_back(std::in_place_type<dot_conc>, adjust_range({ start, cur_pos() }));
                    start = cur_pos_adjusted();
                    break;

                case U'=':
                    dump_s();
                    start = cur_pos_adjusted();
                    consume();
                    cc.emplace_back(std::in_place_type<equals_conc>, adjust_range({ start, cur_pos() }));
                    start = cur_pos_adjusted();
                    break;

                case U'&':
                    if (input.next[1] == U'&')
                    {
                        consume_into(s);
                        consume();
                    }
                    else
                    {
                        dump_s();
                        auto [error, vs] = lex_variable();
                        if (error)
                            return failure;
                        cc.emplace_back(std::in_place_type<var_sym_conc>, std::move(vs));
                        start = cur_pos_adjusted();
                    }
                    break;

                case U'\'':
                    if (input.next[1] != U'\'')
                        goto done;
                    consume_into(s);
                    consume();
                    break;

                default:
                    consume_into(s);
                    break;
            }
        }
    done:;
        if (!match<hl_scopes::operator_symbol, U'\''>(diagnostic_op::error_S0005))
            return failure;
        concatenation_point::clear_concat_chain(cc);
        return cc;
    }

    result_t<expressions::ca_string::substring_t> lex_substring()
    {
        assert(follows<U'('>());

        const auto sub_start = cur_pos_adjusted();

        consume<hl_scopes::operator_symbol>();

        auto [e1_error, e1] = lex_expr_general();
        if (e1_error)
            return failure;

        if (!match<hl_scopes::operator_symbol, U','>())
            return failure;

        if (follows<U'*'>())
        {
            consume(); // TODO: no hightlighting?
            if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
                return failure;
            return { std::move(e1), ca_expr_ptr(), adjust_range({ sub_start, cur_pos() }) };
        }

        auto [e2_error, e2] = lex_expr_general();
        if (e2_error)
            return failure;

        if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
            return failure;

        return {
            std::move(e1),
            std::move(e2),
            adjust_range({ sub_start, cur_pos() }),
        };
    }

    result_t<std::pair<concat_chain, expressions::ca_string::substring_t>> lex_ca_string_with_optional_substring()
    {
        assert(follows<U'\''>());
        auto [cc_error, cc] = lex_ca_string_value();
        if (cc_error)
            return failure;

        if (!follows<U'('>())
            return { std::move(cc), expressions::ca_string::substring_t {} };

        auto [sub_error, sub] = lex_substring();
        if (sub_error)
            return failure;

        return { std::move(cc), std::move(sub) };
    }

    bool lex_optional_space()
    {
        bool matched = false;
        while (follows<U' '>())
        {
            consume();
            matched = true;
        }
        return matched;
    }

    result_t<std::vector<ca_expr_ptr>> lex_subscript_ne()
    {
        assert(follows<U'('>());

        std::vector<ca_expr_ptr> result;

        consume<hl_scopes::operator_symbol>();
        if (lex_optional_space())
        {
            auto [error, e] = lex_expr();
            if (error)
                return failure;

            result.push_back(std::move(e));
            lex_optional_space();
            if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
                return failure;

            return result;
        }

        if (auto [error, e] = lex_expr(); error)
            return failure;
        else
            result.push_back(std::move(e));

        if (!match<hl_scopes::operator_symbol, U','>(diagnostic_op::error_S0002))
            return failure;

        if (auto [error, e] = lex_expr(); error)
            return failure;
        else
            result.push_back(std::move(e));

        while (follows<U','>())
        {
            consume<hl_scopes::operator_symbol>();
            if (auto [error, e] = lex_expr(); error)
                return failure;
            else
                result.push_back(std::move(e));
        }
        if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
            return failure;

        return result;
    }

    [[nodiscard]] auto parse_self_def_term(std::string_view type, std::string_view value, range r)
    {
        return holder->parser->parse_self_def_term(type, value, r);
    }
    [[nodiscard]] auto parse_self_def_term_in_mach(std::string_view type, std::string_view value, range r)
    {
        return holder->parser->parse_self_def_term_in_mach(type, value, r);
    }

    [[nodiscard]] auto allow_literals() const { return holder->parser->allow_literals(); }
    [[nodiscard]] auto disable_literals() const { return holder->parser->disable_literals(); }

    result_t<ca_expr_ptr> lex_term()
    {
        const auto start = cur_pos_adjusted();
        switch (*input.next)
        {
            case EOF_SYMBOL:
                add_diagnostic(diagnostic_op::error_S0003);
                return failure;

            case U'&':
                if (auto [error, v] = lex_variable(); error)
                    return failure;
                else
                    return std::make_unique<ca_var_sym>(std::move(v), adjust_range({ start, cur_pos() }));

            case U'-':
            case U'0':
            case U'1':
            case U'2':
            case U'3':
            case U'4':
            case U'5':
            case U'6':
            case U'7':
            case U'8':
            case U'9': {
                const auto [error, number] = lex_number_as_string();
                if (error)
                    return failure;
                const auto& [v, r] = number;
                return std::make_unique<ca_constant>(parse_self_def_term("D", v, r), r);
            }

            case U'\'':
                if (auto [error, s] = lex_ca_string_with_optional_substring(); error)
                    return failure;
                else
                {
                    ca_expr_ptr result = std::make_unique<expressions::ca_string>(
                        std::move(s.first), ca_expr_ptr(), std::move(s.second), adjust_range({ start, cur_pos() }));

                    while (follows<U'(', U'\''>())
                    {
                        const auto conc_start = cur_pos_adjusted();
                        ca_expr_ptr nested_dupl;
                        if (follows<U'('>())
                        {
                            consume<hl_scopes::operator_symbol>();
                            if (auto [error2, dupl] = lex_expr_general(); error2)
                                return failure;
                            else if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
                                return failure;
                            else
                                nested_dupl = std::move(dupl);
                        }
                        if (auto [error2, s2] = lex_ca_string_with_optional_substring(); error2)
                            return failure;
                        else
                        {
                            auto next = std::make_unique<expressions::ca_string>(std::move(s2.first),
                                std::move(nested_dupl),
                                std::move(s2.second),
                                adjust_range({ conc_start, cur_pos() }));
                            result = std::make_unique<ca_basic_binary_operator<ca_conc>>(
                                std::move(result), std::move(next), adjust_range({ start, cur_pos() }));
                        }
                    }
                    return result;
                }

            case U'(': {
                consume<hl_scopes::operator_symbol>();
                if (eof())
                {
                    add_diagnostic(diagnostic_op::error_S0003);
                    return failure;
                }

                ca_expr_ptr p_expr;
                if (!follows_NOT_SPACE())
                {
                    std::vector<ca_expr_ptr> expr_list;
                    auto spaces_found = lex_optional_space();
                    if (auto [error, e] = lex_expr(); error)
                        return failure;
                    else
                        p_expr = std::move(e);
                    spaces_found |= lex_optional_space();
                    for (; except<U')'>(); spaces_found |= lex_optional_space())
                    {
                        if (auto [error, e] = lex_expr(); error)
                            return failure;
                        else
                        {
                            if (p_expr)
                                expr_list.push_back(std::move(p_expr));
                            expr_list.push_back(std::move(e));
                        }
                    }
                    if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
                        return failure;
                    if (spaces_found && p_expr)
                        expr_list.push_back(std::move(p_expr));
                    if (!expr_list.empty())
                        return std::make_unique<ca_expr_list>(
                            std::move(expr_list), adjust_range({ start, cur_pos() }), true);
                }
                else if (auto [error, e] = lex_expr_general(); error)
                    return failure;
                else if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
                    return failure;
                else
                    p_expr = std::move(e);

                if (follows<U'\''>())
                {
                    ca_expr_ptr result;
                    if (auto [error, s] = lex_ca_string_with_optional_substring(); error)
                        return failure;
                    else
                        result = std::make_unique<expressions::ca_string>(std::move(s.first),
                            std::move(p_expr),
                            std::move(s.second),
                            adjust_range({ start, cur_pos() }));
                    while (follows<U'(', U'\''>())
                    {
                        const auto conc_start = cur_pos_adjusted();
                        ca_expr_ptr nested_dupl;
                        if (follows<U'('>())
                        {
                            consume<hl_scopes::operator_symbol>();
                            if (auto [error, dupl] = lex_expr_general(); error)
                                return failure;
                            else if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
                                return failure;
                            else
                                nested_dupl = std::move(dupl);
                        }
                        if (auto [error, s2] = lex_ca_string_with_optional_substring(); error)
                            return failure;
                        else
                        {
                            auto next = std::make_unique<expressions::ca_string>(std::move(s2.first),
                                std::move(nested_dupl),
                                std::move(s2.second),
                                adjust_range({ conc_start, cur_pos() }));
                            result = std::make_unique<ca_basic_binary_operator<ca_conc>>(
                                std::move(result), std::move(next), adjust_range({ start, cur_pos() }));
                        }
                    }
                    return result;
                }
                else if (is_ord_first())
                {
                    id_index id;
                    if (auto [error, id_] = lex_id(); error)
                        return failure;
                    else
                        id = id_;
                    if (!must_follow<U'('>())
                        return failure;
                    if (auto [error, s] = lex_subscript_ne(); error)
                        return failure;
                    else
                    {
                        auto func = ca_common_expr_policy::get_function(id.to_string_view());
                        return std::make_unique<ca_function>(
                            id, func, std::move(s), std::move(p_expr), adjust_range({ start, cur_pos() }));
                    }
                }

                std::vector<ca_expr_ptr> expr_list;
                expr_list.push_back(std::move(p_expr));
                return std::make_unique<ca_expr_list>(std::move(expr_list), adjust_range({ start, cur_pos() }), true);
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
                        case U'g': {
                            const auto c = static_cast<char>(*input.next);
                            consume<hl_scopes::self_def_type>();
                            if (auto [error, s] = lex_simple_string(); error)
                                return failure;
                            else
                            {
                                const auto r = adjust_range({ start, cur_pos() });
                                return std::make_unique<ca_constant>(
                                    parse_self_def_term(std::string_view(&c, 1), std::move(s), r), r);
                            }
                        }

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
                        case U't': {
                            const auto attr =
                                context::symbol_attributes::transform_attr(utils::upper_cased[*input.next]);
                            consume<hl_scopes::data_attr_type>();
                            consume<hl_scopes::operator_symbol>();
                            const auto start_value = cur_pos_adjusted();
                            switch (*input.next)
                            {
                                case EOF_SYMBOL:
                                    add_diagnostic(diagnostic_op::error_S0003);
                                    return failure;
                                case U'&':
                                    if (auto [error, v] = lex_variable(); error)
                                        return failure;
                                    else
                                    {
                                        // TODO:in reality, this seems to be much more complicated (arbitrary many dots
                                        // are consumed for *some* attributes)
                                        if (follows<U'.'>())
                                        {
                                            consume();
                                        }
                                        return std::make_unique<ca_symbol_attribute>(std::move(v),
                                            attr,
                                            adjust_range({ start, cur_pos() }),
                                            adjust_range({ start_value, cur_pos() }));
                                    }
                                case U'=':
                                    if (auto [error, l] = lex_literal(); error)
                                        return failure;
                                    else
                                        return std::make_unique<ca_symbol_attribute>(std::move(l),
                                            attr,
                                            adjust_range({ start, cur_pos() }),
                                            adjust_range({ start_value, cur_pos() }));
                                default:
                                    if (!is_ord_first())
                                    {
                                        add_diagnostic(diagnostic_op::error_S0002);
                                        return failure;
                                    }
                                    if (auto [error, id] = lex_id(); error)
                                        return failure;
                                    else
                                        return std::make_unique<ca_symbol_attribute>(id,
                                            attr,
                                            adjust_range({ start, cur_pos() }),
                                            adjust_range({ start_value, cur_pos() }));
                            }
                        }
                    }
                }
                if (auto [error, id] = lex_id(); error)
                    return failure;
                else if (follows<U'('>())
                {
                    add_hl_symbol<hl_scopes::operand>({ start, cur_pos() });
                    if (auto [error2, s] = lex_subscript_ne(); error2)
                        return failure;
                    else
                        return std::make_unique<ca_function>(id,
                            ca_common_expr_policy::get_function(id.to_string_view()),
                            std::move(s),
                            ca_expr_ptr(),
                            adjust_range({ start, cur_pos() }));
                }
                else
                {
                    const auto r = adjust_range({ start, cur_pos() });
                    add_hl_symbol_adjusted<hl_scopes::operand>(r);
                    return std::make_unique<ca_symbol>(id, r);
                }
        }
    }

    result_t<std::pair<std::string, range>> lex_number_as_string()
    {
        assert((follows<U'0', U'1', U'2', U'3', U'4', U'5', U'6', U'7', U'8', U'9', U'-'>()));
        const auto start = cur_pos_adjusted();

        std::string result;

        if (follows<U'-'>())
        {
            consume_into(result);
        }
        if (!is_num())
        {
            add_diagnostic_or_eof(diagnostic_op::error_S0002);
            return failure;
        }
        do
        {
            consume_into(result);
        } while (is_num());

        const auto r = adjust_range({ start, cur_pos() });
        add_hl_symbol_adjusted<hl_scopes::number>(r);

        return { result, r };
    }

    result_t<mach_expr_ptr> lex_mach_term()
    {
        const auto start = cur_pos_adjusted();
        switch (*input.next)
        {
            case EOF_SYMBOL:
                add_diagnostic(diagnostic_op::error_S0003);
                return failure;

            case U'(':
                consume<hl_scopes::operator_symbol>();
                if (auto [error, e] = lex_mach_expr(); error)
                    return failure;
                else if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
                    return failure;
                else
                    return std::make_unique<mach_expr_unary<par>>(std::move(e), adjust_range({ start, cur_pos() }));

            case U'*':
                consume<hl_scopes::operand>();
                return std::make_unique<mach_expr_location_counter>(adjust_range({ start, cur_pos() }));

            case U'-':
            case U'0':
            case U'1':
            case U'2':
            case U'3':
            case U'4':
            case U'5':
            case U'6':
            case U'7':
            case U'8':
            case U'9': {
                const auto [error, number] = lex_number_as_string();
                if (error)
                    return failure;
                const auto& [v, r] = number;
                return std::make_unique<mach_expr_constant>(parse_self_def_term_in_mach("D", v, r), r);
            }

            case U'=':
                if (auto [error, l] = lex_literal(); error)
                    return failure;
                else
                    return std::make_unique<mach_expr_literal>(adjust_range({ start, cur_pos() }), std::move(l));

            default:
                if (!is_ord_first())
                {
                    add_diagnostic(diagnostic_op::error_S0002);
                    return failure;
                }
                if (follows<U'C', U'c'>() && (input.next[1] == U'A' || input.next[1] == U'a') && input.next[2] == U'\'')
                {
                    consume();
                    consume();
                    add_hl_symbol<hl_scopes::self_def_type>({ start, cur_pos() });

                    if (auto [error, s] = lex_mach_string(); error)
                        return failure;
                    else
                    {
                        const auto r = adjust_range({ start, cur_pos() });
                        return std::make_unique<mach_expr_constant>(
                            parse_self_def_term_in_mach("CA", std::move(s), r), r);
                    }
                }
                if (input.next[1] == U'\'')
                {
                    switch (*input.next)
                    {
                        case U'L':
                        case U'l':
                            if (input.next[2] == U'*')
                            {
                                consume<hl_scopes::data_attr_type>();
                                consume<hl_scopes::operator_symbol>();
                                if (!holder->parser->proc_status.has_value())
                                {
                                    add_diagnostic(diagnostic_op::error_S0002);
                                    return failure;
                                }
                                consume<hl_scopes::operand>();
                                return std::make_unique<mach_expr_constant>(
                                    holder->parser->get_loctr_len(), adjust_range({ start, cur_pos() }));
                            }
                            [[fallthrough]];
                        case U'O':
                        case U'S':
                        case U'I':
                        case U'T':
                        case U'o':
                        case U's':
                        case U'i':
                        case U't': {
                            const auto attr =
                                context::symbol_attributes::transform_attr(utils::upper_cased[*input.next]);
                            consume<hl_scopes::data_attr_type>();
                            consume<hl_scopes::operator_symbol>();
                            const auto start_value = cur_pos_adjusted();
                            if (follows<U'='>())
                            {
                                auto [error, l] = lex_literal();
                                if (error)
                                    return failure;

                                return std::make_unique<mach_expr_data_attr_literal>(
                                    std::make_unique<mach_expr_literal>(
                                        adjust_range({ start_value, cur_pos() }), std::move(l)),
                                    attr,
                                    adjust_range({ start, cur_pos() }),
                                    adjust_range({ start_value, cur_pos() }));
                            }
                            else if (is_ord_first())
                            {
                                auto [error, q_id] = lex_qualified_id();
                                if (error)
                                    return failure;
                                add_hl_symbol<hl_scopes::ordinary_symbol>({ start, cur_pos() });
                                return std::make_unique<mach_expr_data_attr>(q_id.first,
                                    q_id.first,
                                    attr,
                                    adjust_range({ start, cur_pos() }),
                                    adjust_range({ start_value, cur_pos() }));
                            }
                            else
                            {
                                add_diagnostic(diagnostic_op::error_S0002);
                                return failure;
                            }
                            break;
                        }

                        case U'B':
                        case U'D':
                        case U'X':
                        case U'C':
                        case U'b':
                        case U'd':
                        case U'x':
                        case U'c': {
                            const auto opt = static_cast<char>(*input.next);
                            consume<hl_scopes::self_def_type>();
                            auto [error, s] = lex_mach_string();
                            if (error)
                                return failure;

                            const auto r = adjust_range({ start, cur_pos() });
                            return std::make_unique<mach_expr_constant>(
                                parse_self_def_term_in_mach(std::string_view(&opt, 1), s, r), r);
                        }
                    }
                }
                if (!is_ord_first())
                {
                    add_diagnostic(diagnostic_op::error_S0002);
                    return failure;
                }
                if (auto [error, id] = lex_id(); error)
                    return failure;
                else if (follows<U'.'>())
                {
                    consume<hl_scopes::operator_symbol>();
                    if (!is_ord_first())
                    {
                        add_diagnostic(diagnostic_op::error_S0002);
                        return failure;
                    }

                    if (auto [error2, id2] = lex_id(); error2)
                        return failure;
                    else
                    {
                        const auto r = adjust_range({ start, cur_pos() });
                        add_hl_symbol_adjusted<hl_scopes::ordinary_symbol>(r);
                        return std::make_unique<mach_expr_symbol>(id2, id, r);
                    }
                }
                else
                {
                    const auto r = adjust_range({ start, cur_pos() });
                    add_hl_symbol_adjusted<hl_scopes::ordinary_symbol>(r);
                    return std::make_unique<mach_expr_symbol>(id, id_index(), r);
                }
        }
    }

    result_t<std::string> lex_mach_string()
    {
        assert(follows<U'\''>());

        const auto start = cur_pos_adjusted();
        std::string s;

        consume();

        while (!eof())
        {
            if (input.next[0] != U'\'')
            {
                consume_into(s);
            }
            else if (input.next[1] == U'\'')
            {
                consume_into(s);
                consume();
            }
            else
            {
                consume();
                add_hl_symbol<hl_scopes::string>({ start, cur_pos() });
                return s;
            }
        }

        add_diagnostic(diagnostic_op::error_S0005);
        return failure;
    }

    result_t<mach_expr_ptr> lex_mach_term_c()
    {
        if (follows<U'+'>() || (follows<U'-'>() && !is_num(input.next[1])))
        {
            const auto plus = *input.next == U'+';
            const auto start = cur_pos_adjusted();
            consume<hl_scopes::operator_symbol>();
            if (auto [error, e] = lex_mach_term_c(); error)
                return failure;
            else if (plus)
                return std::make_unique<mach_expr_unary<add>>(std::move(e), adjust_range({ start, cur_pos() }));
            else
                return std::make_unique<mach_expr_unary<sub>>(std::move(e), adjust_range({ start, cur_pos() }));
        }

        return lex_mach_term();
    }

    result_t<mach_expr_ptr> lex_mach_expr_s()
    {
        const auto start = cur_pos_adjusted();
        if (auto [error, e] = lex_mach_term_c(); error)
            return failure;
        else
        {
            while (follows<U'*', U'/'>())
            {
                const auto mul = *input.next == U'*';
                consume<hl_scopes::operator_symbol>();
                if (auto [error2, next] = lex_mach_term_c(); error2)
                    return failure;
                else if (mul)
                    e = std::make_unique<mach_expr_binary<expressions::mul>>(
                        std::move(e), std::move(next), adjust_range({ start, cur_pos() }));
                else
                    e = std::make_unique<mach_expr_binary<div>>(
                        std::move(e), std::move(next), adjust_range({ start, cur_pos() }));
            }
            return std::move(e);
        }
    }

    result_t<mach_expr_ptr> lex_mach_expr()
    {
        const auto start = cur_pos_adjusted();
        if (auto [error, e] = lex_mach_expr_s(); error)
            return failure;
        else
        {
            while (follows<U'+', U'-'>())
            {
                const auto plus = *input.next == U'+';
                consume<hl_scopes::operator_symbol>();
                if (auto [error2, next] = lex_mach_expr_s(); error2)
                    return failure;
                else if (plus)
                    e = std::make_unique<mach_expr_binary<add>>(
                        std::move(e), std::move(next), adjust_range({ start, cur_pos() }));
                else
                    e = std::make_unique<mach_expr_binary<sub>>(
                        std::move(e), std::move(next), adjust_range({ start, cur_pos() }));
            }
            return std::move(e);
        }
    }

    static bool is_type_extension(char type, char ch)
    {
        return checking::data_def_type::types_and_extensions.count(std::make_pair(type, ch)) > 0;
    }

    static constexpr unsigned char digit_to_value(char32_t c)
    {
        static_assert(U'0' + 0 == U'0');
        static_assert(U'0' + 1 == U'1');
        static_assert(U'0' + 2 == U'2');
        static_assert(U'0' + 3 == U'3');
        static_assert(U'0' + 4 == U'4');
        static_assert(U'0' + 5 == U'5');
        static_assert(U'0' + 6 == U'6');
        static_assert(U'0' + 7 == U'7');
        static_assert(U'0' + 8 == U'8');
        static_assert(U'0' + 9 == U'9');
        // [[assume(c >= U'0' && c <= U'9')]];
        assert(c >= U'0' && c <= U'9');
        return c - U'0';
    }

    result_t<std::pair<int32_t, range>> parse_number()
    {
        constexpr long long min_l = -(1LL << 31);
        constexpr long long max_l = (1LL << 31) - 1;
        constexpr long long parse_limit_l = (1LL << 31);
        static_assert(std::numeric_limits<int32_t>::min() <= min_l);
        static_assert(std::numeric_limits<int32_t>::max() >= max_l);

        const auto start = cur_pos_adjusted();

        long long result = 0;
        const bool negative = [&]() {
            switch (*input.next)
            {
                case U'-':
                    consume();
                    return true;
                case U'+':
                    consume();
                    return false;
                default:
                    return false;
            }
        }();

        bool parsed_one = false;
        while (!eof())
        {
            if (!is_num())
                break;
            const auto c = *input.next;
            parsed_one = true;

            consume();

            if (result > parse_limit_l)
                continue;

            result = result * 10 + digit_to_value(c);
        }
        const auto r = adjust_range({ start, cur_pos() });
        if (!parsed_one)
        {
            add_diagnostic(diagnostic_op::error_D002(r));
            return failure;
        }
        if (negative)
            result = -result;
        if (result < min_l || result > max_l)
        {
            add_diagnostic(diagnostic_op::error_D001(r));
            return failure;
        }
        add_hl_symbol_adjusted<hl_scopes::number>(r);

        return { (int32_t)result, r };
    }

    result_t<mach_expr_ptr> lex_literal_signed_num()
    {
        if (follows<U'('>())
        {
            consume<hl_scopes::operator_symbol>();
            if (auto [error, e] = lex_mach_expr(); error)
                return failure;
            else if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
                return failure;
            else
                return std::move(e);
        }
        else if (auto [error, n] = parse_number(); error)
            return failure;
        else
            return std::make_unique<mach_expr_constant>(n.first, n.second);
    }

    result_t<mach_expr_ptr> lex_literal_unsigned_num()
    {
        if (follows<U'('>())
        {
            consume<hl_scopes::operator_symbol>();
            if (auto [error, e] = lex_mach_expr(); error)
                return failure;
            else if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
                return failure;
            else
                return std::move(e);
        }
        else if (!is_num())
        {
            add_diagnostic(diagnostic_op::error_S0002);
            return failure;
        }
        else if (auto [error, n] = parse_number(); error)
            return failure;
        else
            return std::make_unique<mach_expr_constant>(n.first, n.second);
    }

    result_t<data_definition> lex_data_def_base()
    {
        const auto goff = holder->parser->goff();

        data_definition result;
        // case state::duplicating_factor:
        if (follows<U'('>() || is_num())
        {
            if (auto [error, e] = lex_literal_unsigned_num(); error)
                return failure;
            else
                result.dupl_factor = std::move(e);
        }

        // case state::read_type:
        if (!is_ord_first())
        {
            add_diagnostic(diagnostic_op::error_S0002);
            return failure;
        }
        const auto type = utils::upper_cased[*input.next];
        consume<hl_scopes::data_def_type>();
        const auto type_start = cur_pos_adjusted();
        consume();

        result.type = type == 'R' && !goff ? 'r' : type;
        result.type_range = adjust_range({ type_start, cur_pos() });
        if (is_ord_first() && is_type_extension(type, utils::upper_cased[*input.next]))
        {
            result.extension = utils::upper_cased[*input.next];
            const auto ext_start = cur_pos_adjusted();
            consume();
            result.extension_range = adjust_range({ ext_start, cur_pos() });
        }
        add_hl_symbol<hl_scopes::data_def_type>(adjust_range({ type_start, cur_pos() }));

        // case state::try_reading_program:
        // case state::read_program:
        if (const auto c = *input.next; c == U'P' || c == U'p')
        {
            consume<hl_scopes::data_def_modifier>();
            if (auto [error, e] = lex_literal_signed_num(); error)
                return failure;
            else
                result.program_type = std::move(e);
        }
        // case state::try_reading_length:
        // case state::try_reading_bitfield:
        // case state::read_length:
        if (const auto c = *input.next; c == U'L' || c == U'l')
        {
            consume<hl_scopes::data_def_modifier>();
            if (follows<U'.'>())
            {
                result.length_type = data_definition::length_type::BIT;
                consume();
            }
            if (auto [error, e] = lex_literal_unsigned_num(); error)
                return failure;
            else
                result.length = std::move(e);
        }

        // case state::try_reading_scale:
        // case state::read_scale:
        if (const auto c = *input.next; c == U'S' || c == U's')
        {
            consume<hl_scopes::data_def_modifier>();
            if (auto [error, e] = lex_literal_signed_num(); error)
                return failure;
            else
                result.scale = std::move(e);
        }

        // case state::try_reading_exponent:
        // case state::read_exponent:
        if (const auto c = *input.next; c == U'E' || c == U'e')
        {
            consume<hl_scopes::data_def_modifier>();
            if (auto [error, e] = lex_literal_signed_num(); error)
                return failure;
            else
                result.exponent = std::move(e);
        }
        return result;
    }

    result_t<expressions::expr_or_address> lex_expr_or_addr()
    {
        const auto start = cur_pos_adjusted();
        if (auto [error, e] = lex_mach_expr(); error)
            return failure;
        else if (follows<U'('>())
        {
            consume<hl_scopes::operator_symbol>();
            if (auto [error2, e2] = lex_mach_expr(); error2)
                return failure;
            else if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
                return failure;
            else
                return expressions::expr_or_address(std::in_place_type<expressions::address_nominal>,
                    std::move(e),
                    std::move(e2),
                    adjust_range({ start, cur_pos() }));
        }
        else
            return { std::move(e) };
    }


    result_t<std::string> lex_literal_nominal_char()
    {
        assert(follows<U'\''>());
        const auto start = cur_pos_adjusted();

        std::string result;
        consume();
        while (true)
        {
            if (input.next[0] == U'\'' && input.next[1] == U'\'')
            {
                consume_into(result);
                consume();
            }
            else if (except<U'\''>())
            {
                consume_into(result);
            }
            else
                break;
        }
        if (!match<U'\''>(diagnostic_op::error_S0005))
            return failure;

        add_hl_symbol<hl_scopes::string>({ start, cur_pos() });

        return result;
    }


    result_t<expr_or_address_list> lex_literal_nominal_addr()
    {
        assert(follows<U'('>());
        consume<hl_scopes::operator_symbol>();

        expr_or_address_list result;

        if (auto [error, e] = lex_expr_or_addr(); error)
            return failure;
        else
            result.push_back(std::move(e));


        while (follows<U','>())
        {
            consume<hl_scopes::operator_symbol>();
            if (auto [error, e] = lex_expr_or_addr(); error)
                return failure;
            else
                result.push_back(std::move(e));
        }

        if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
            return failure;

        return result;
    }

    result_t<nominal_value_ptr> lex_literal_nominal()
    {
        const auto start = cur_pos_adjusted();
        if (follows<U'\''>())
        {
            if (auto [error, n] = lex_literal_nominal_char(); error)
                return failure;
            else
                return std::make_unique<nominal_value_string>(std::move(n), adjust_range({ start, cur_pos() }));
        }
        else if (follows<U'('>())
        {
            if (auto [error, n] = lex_literal_nominal_addr(); error)
                return failure;
            else
                return std::make_unique<nominal_value_exprs>(std::move(n));
        }
        else
        {
            add_diagnostic(diagnostic_op::error_S0003);
            return failure;
        }
    }

    result_t<literal_si> lex_literal()
    {
        const auto allowed = allow_literals();
        const auto disabled = disable_literals();
        const auto start = cur_pos_adjusted();
        const auto initial = input.next;

        assert(follows<U'='>());
        consume<hl_scopes::operator_symbol>();

        if (auto [error, d] = lex_data_def_base(); error)
            return failure;
        else if (auto [error2, n] = lex_literal_nominal(); error2)
            return failure;
        else if (!allowed)
        {
            add_diagnostic(diagnostic_op::error_S0013);
            return failure;
        }
        else
        {
            d.nominal_value = std::move(n);
            std::string s;
            s.reserve(input.next - initial);
            std::for_each(initial, input.next, std::bind_front(utils::append_utf32_to_utf8, s));
            return holder->parser->get_collector().add_literal(
                std::move(s), std::move(d), adjust_range({ start, cur_pos() }));
        }
    }

    result_t<std::string> lex_simple_string()
    {
        assert(follows<U'\''>());

        std::string result;
        const auto start = cur_pos_adjusted();

        consume();
        while (!eof())
        {
            switch (*input.next)
            {
                case U'&':
                    if (input.next[1] != U'&')
                    {
                        add_diagnostic(diagnostic_op::error_S0002);
                        return failure;
                    }
                    consume_into(result);
                    consume();
                    break;
                case U'\'':
                    if (input.next[1] != U'\'')
                        goto done;
                    consume_into(result);
                    consume();
                    break;
                default:
                    consume_into(result);
                    break;
            }
        }
    done:;
        if (*input.next != U'\'')
        {
            add_diagnostic(diagnostic_op::error_S0005);
            return failure;
        }
        consume();
        add_hl_symbol<hl_scopes::string>({ start, cur_pos() });

        return result;
    }

    result_t<ca_expr_ptr> lex_term_c()
    {
        if (input.next[0] == U'+' || (input.next[0] == U'-' && !is_num(input.next[1])))
        {
            const auto start = cur_pos_adjusted();
            const auto plus = *input.next == U'+';
            if (auto [error, e] = lex_term(); error)
                return failure;
            else if (plus)
                return std::make_unique<ca_plus_operator>(std::move(e), adjust_range({ start, cur_pos() }));
            else
                return std::make_unique<ca_minus_operator>(std::move(e), adjust_range({ start, cur_pos() }));
        }
        return lex_term();
    }

    result_t<ca_expr_ptr> lex_expr_s()
    {
        ca_expr_ptr result;
        const auto start = cur_pos_adjusted();
        if (auto [error, e] = lex_term_c(); error)
            return failure;
        else
            result = std::move(e);

        while (follows<U'*', U'/'>())
        {
            const auto mult = *input.next == U'*';
            consume<hl_scopes::operator_symbol>();
            if (auto [error, e] = lex_term_c(); error)
                return failure;
            else if (mult)
                result = std::make_unique<ca_basic_binary_operator<ca_mul>>(
                    std::move(result), std::move(e), adjust_range({ start, cur_pos() }));
            else
                result = std::make_unique<ca_basic_binary_operator<ca_div>>(
                    std::move(result), std::move(e), adjust_range({ start, cur_pos() }));
        }

        return result;
    }

    result_t<ca_expr_ptr> lex_expr()
    {
        ca_expr_ptr result;
        const auto start = cur_pos_adjusted();

        if (auto [error, e] = lex_expr_s(); error)
            return failure;
        else
            result = std::move(e);

        switch (*input.next)
        {
            case '+':
            case '-':
                while (follows<U'+', U'-'>())
                {
                    const auto plus = *input.next == U'+';
                    consume<hl_scopes::operator_symbol>();
                    if (auto [error, e] = lex_expr_s(); error)
                        return failure;
                    else if (plus)
                        result = std::make_unique<ca_basic_binary_operator<ca_add>>(
                            std::move(result), std::move(e), adjust_range({ start, cur_pos() }));
                    else
                        result = std::make_unique<ca_basic_binary_operator<ca_sub>>(
                            std::move(result), std::move(e), adjust_range({ start, cur_pos() }));
                }
                break;
            case '.':
                while (follows<U'.'>())
                {
                    consume<hl_scopes::operator_symbol>();
                    if (auto [error, e] = lex_term_c(); error)
                        return failure;
                    else
                        result = std::make_unique<ca_basic_binary_operator<ca_conc>>(
                            std::move(result), std::move(e), adjust_range({ start, cur_pos() }));
                }
                break;
        }

        return result;
    }

    result_t<std::vector<ca_expr_ptr>> lex_subscript()
    {
        assert(follows<U'('>());

        consume<hl_scopes::operator_symbol>();

        std::vector<ca_expr_ptr> result;

        if (auto [error, expr] = lex_expr(); error)
            return failure;
        else
            result.push_back(std::move(expr));

        while (follows<U','>())
        {
            consume<hl_scopes::operator_symbol>();
            if (auto [error, expr] = lex_expr(); error)
                return failure;
            else
                result.push_back(std::move(expr));
        }

        if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
            return failure;

        return result;
    }

    result_t_void lex_macro_operand(concat_chain& cc, bool next_char_special)
    {
        char_str_conc* last_text_state = nullptr;
        const auto last_text = [&]() {
            if (last_text_state)
                return last_text_state;
            last_text_state = &std::get<char_str_conc>(
                cc.emplace_back(std::in_place_type<char_str_conc>, std::string(), range(cur_pos_adjusted())).value);
            return last_text_state;
        };
        const auto push_last_text = [&]() {
            if (!last_text_state)
                return;
            last_text_state->conc_range = adjust_range({ last_text_state->conc_range.start, cur_pos() });
            add_hl_symbol_adjusted<hl_scopes::operand>(last_text_state->conc_range);
            last_text_state = nullptr;
        };
        const auto single_char_push = [&]<typename T>(std::in_place_type_t<T> t) {
            const auto s = cur_pos_adjusted();
            consume();
            const auto r = adjust_range({ s, cur_pos() });
            cc.emplace_back(t, r);
            last_text_state = nullptr;

            return r;
        };
        while (true)
        {
            const bool last_char_special = std::exchange(next_char_special, true);
            switch (*input.next)
            {
                case U'(': {
                    std::vector<concat_chain> nested;
                    push_last_text();
                    if (auto [error] = process_macro_list(nested); error)
                        return failure;
                    cc.emplace_back(std::in_place_type<sublist_conc>, std::move(nested));
                    break;
                }

                case U'=':
                    push_last_text();
                    add_hl_symbol_adjusted<hl_scopes::operator_symbol>(
                        single_char_push(std::in_place_type<equals_conc>));
                    break;

                case U'.':
                    push_last_text();
                    add_hl_symbol_adjusted<hl_scopes::operator_symbol>(single_char_push(std::in_place_type<dot_conc>));
                    break;

                case EOF_SYMBOL:
                case U' ':
                case U')':
                case U',':
                    push_last_text();
                    return {};

                case U'\'': {
                    consume_into(last_text()->value);
                    while (!eof())
                    {
                        switch (*input.next)
                        {
                            case U'\'':
                                if (input.next[1] != U'\'')
                                    goto done;
                                consume_into<2>(last_text()->value); // TODO: Why two quotes?
                                break;
                            case U'&':
                                if (input.next[1] == U'&')
                                {
                                    consume_into<2>(last_text()->value);
                                    break;
                                }
                                push_last_text();
                                if (auto [error, vs] = lex_variable(); error)
                                    return failure;
                                else
                                    cc.emplace_back(std::in_place_type<var_sym_conc>, std::move(vs));
                                break;

                            case U'=':
                                push_last_text();
                                single_char_push(std::in_place_type<equals_conc>);
                                break;

                            case U'.':
                                push_last_text();
                                single_char_push(std::in_place_type<dot_conc>);
                                break;

                            default:
                                consume_into(last_text()->value);
                                break;
                        }
                    }
                done:;
                    if (!must_follow<U'\''>(diagnostic_op::error_S0005))
                    {
                        push_last_text();
                        return failure;
                    }
                    consume_into(last_text()->value);
                    push_last_text();
                    next_char_special = false;
                    break;
                }

                case U'&':
                    if (input.next[1] == U'&')
                    {
                        consume_into<2>(last_text()->value);
                        // add_hl_symbol_adjusted<hl_scopes::???>(r);
                        break;
                    }
                    push_last_text();
                    if (auto [error, v] = lex_variable(); error)
                        return failure;
                    else
                        cc.emplace_back(std::in_place_type<var_sym_conc>, std::move(v));
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
                        consume_into(last_text()->value);
                        next_char_special = false;
                        break;
                    }
                    else if (is_ord_first(input.next[2]) || input.next[2] == U'=')
                    {
                        consume_into<2>(last_text()->value);
                        next_char_special = false;
                        break;
                    }
                    else if (input.next[2] != U'&')
                    {
                        consume_into(last_text()->value);
                        next_char_special = false;
                        break;
                    }

                    while (except<U',', U')', U' '>())
                    {
                        if (*input.next != U'&')
                        {
                            consume_into(last_text()->value);
                        }
                        else if (input.next[1] == U'&')
                        {
                            consume_into<2>(last_text()->value);
                        }
                        else if (auto [error, vs] = (push_last_text(), lex_variable()); error)
                            return failure;
                        else
                        {
                            cc.emplace_back(std::in_place_type<var_sym_conc>, std::move(vs));
                            if (follows<U'.'>())
                                add_hl_symbol_adjusted<hl_scopes::operator_symbol>(
                                    single_char_push(std::in_place_type<dot_conc>));
                        }
                    }
                    break;

                default:
                    next_char_special = *input.next >= ord.size() || !ord[*input.next];
                    consume_into(last_text()->value);
                    break;
            }
        }
    }

    void process_optional_line_remark()
    {
        if (follows<U' '>() && before_nl())
        {
            lex_line_remark();

            adjust_lines();
        }
    }

    result_t_void process_macro_list(std::vector<concat_chain>& cc)
    {
        assert(follows<U'('>());

        consume<hl_scopes::operator_symbol>();
        if (follows<U')'>())
        {
            consume<hl_scopes::operator_symbol>();
            return {};
        }

        if (auto [error] = lex_macro_operand(cc.emplace_back(), true); error)
            return failure;

        while (follows<U','>())
        {
            consume<hl_scopes::operator_symbol>();
            process_optional_line_remark();
            if (auto [error] = lex_macro_operand(cc.emplace_back(), true); error)
                return failure;
        }

        if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
            return failure;

        return {};
    }

    std::pair<operand_list, range> macro_ops(bool reparse)
    {
        const auto input_start = cur_pos_adjusted();
        if (eof())
            return { operand_list(), adjust_range(range(input_start)) };

        if (!reparse && *input.next != U' ')
        {
            add_diagnostic(diagnostic_op::error_S0002);
            consume_rest();
            return { operand_list(), adjust_range({ input_start, cur_pos() }) };
        }

        // skip spaces
        while (follows<U' '>())
            consume();
        adjust_lines();

        if (eof())
            return { operand_list(), adjust_range(range(cur_pos())) };

        operand_list result;

        auto line_start = cur_pos(); // already adjusted
        auto start = line_start;
        concat_chain cc;
        bool pending = true;

        const auto push_operand = [&]() {
            if (!pending)
                return;
            if (cc.empty())
                result.push_back(std::make_unique<semantics::empty_operand>(adjust_range({ start, cur_pos() })));
            else
                result.push_back(std::make_unique<macro_operand>(std::move(cc), adjust_range({ start, cur_pos() })));
        };

        // process operands
        while (!eof())
        {
            switch (*input.next)
            {
                case U' ':
                    push_operand();
                    pending = false;
                    lex_last_remark();
                    goto end;

                case U',':
                    push_operand();
                    consume<hl_scopes::operator_symbol>();
                    process_optional_line_remark();
                    start = cur_pos_adjusted();
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
                        if (auto [error] = lex_macro_operand(cc, true); error)
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
                case U'Z': {
                    bool next_char_special = false;
                    auto& l = std::get<char_str_conc>(
                        cc.emplace_back(std::in_place_type<char_str_conc>, std::string(), range(cur_pos_adjusted()))
                            .value);
                    while (is_ord())
                    {
                        l.value.push_back(static_cast<char>(*input.next));
                        consume();
                    }
                    l.conc_range.end = cur_pos();
                    l.conc_range = adjust_range(l.conc_range);
                    add_hl_symbol_adjusted<hl_scopes::operand>(l.conc_range);
                    if (follows<U'='>())
                    {
                        const auto s = cur_pos_adjusted();
                        consume();
                        cc.emplace_back(std::in_place_type<equals_conc>, adjust_range({ s, cur_pos() }));
                        next_char_special = true;
                    }
                    if (const auto n = *input.next; n == EOF_SYMBOL || n == U' ' || n == U',')
                        continue;
                    if (auto [error] = lex_macro_operand(cc, next_char_special); error)
                    {
                        consume_rest();
                        goto end;
                    }
                    break;
                }

                case U'\'':
                case U'&':
                default:
                    if (auto [error] = lex_macro_operand(cc, true); error)
                    {
                        consume_rest();
                        goto end;
                    }
                    break;

                case U')':
                    add_diagnostic(diagnostic_op::error_S0012);
                    consume_rest();
                    goto end;

                case U'(': {
                    std::vector<concat_chain> nested;
                    if (auto [error] = process_macro_list(nested); error)
                    {
                        consume_rest();
                        goto end;
                    }
                    cc.emplace_back(std::in_place_type<sublist_conc>, std::move(nested));
                    break;
                }
            }
        }
    end:;
        push_operand();

        return { std::move(result), adjust_range({ line_start, cur_pos() }) };
    }

    macro_preprocessor_t(const parser_holder* h)
        : holder(h)
        , cont(h->lex->get_continuation_column())
    {
        std::tie(input, data) = holder->lex->peek_initial_input_state();
    }
};

parser_holder::macro_preprocessor_t::result_t<vs_ptr> parser_holder::macro_preprocessor_t::lex_variable()
{
    assert(follows<U'&'>());

    const auto start = cur_pos_adjusted();
    consume();

    concat_chain cc;
    id_index id;
    if (follows<U'('>())
    {
        add_hl_symbol<hl_scopes::var_symbol>({ start, cur_pos() });
        consume<hl_scopes::operator_symbol>();
        if (auto [error, cc_] = lex_compound_variable(); error)
            return failure;
        else
            cc = std::move(cc_);

        if (!match<hl_scopes::operator_symbol, U')'>(diagnostic_op::error_S0011))
            return failure;
    }
    else if (!is_ord_first())
    {
        add_diagnostic(diagnostic_op::error_S0008);
        return failure;
    }
    else
    {
        if (auto [error, id_] = lex_id(); error)
            return failure;
        else
            id = id_;
        add_hl_symbol<hl_scopes::var_symbol>({ start, cur_pos() });
    }

    std::vector<ca_expr_ptr> sub;
    if (follows<U'('>())
    {
        if (auto [error, sub_] = lex_subscript(); error)
            return failure;
        else
            sub = std::move(sub_);
    }

    const auto end = cur_pos();

    if (!id.empty())
        return std::make_unique<basic_variable_symbol>(id, std::move(sub), adjust_range({ start, end }));
    else
        return std::make_unique<created_variable_symbol>(std::move(cc), std::move(sub), adjust_range({ start, end }));
}

semantics::operand_list parser_holder::macro_ops(bool reparse) const
{
    parser_holder::macro_preprocessor_t p(this);
    auto [ops, line_range] = p.macro_ops(reparse);

    if (!reparse)
    {
        parser->collector.set_operand_remark_field(std::move(ops), std::move(p.remarks), line_range);
        return {};
    }
    else
        return std::move(ops);
}

} // namespace hlasm_plugin::parser_library::parsing
