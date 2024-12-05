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

#include <algorithm>
#include <cctype>
#include <charconv>
#include <concepts>
#include <utility>

#include "context/hlasm_context.h"
#include "context/literal_pool.h"
#include "expressions/conditional_assembly/ca_expr_policy.h"
#include "expressions/conditional_assembly/ca_expr_visitor.h"
#include "expressions/conditional_assembly/ca_expression.h"
#include "expressions/conditional_assembly/ca_operator_binary.h"
#include "expressions/conditional_assembly/ca_operator_unary.h"
#include "expressions/conditional_assembly/terms/ca_constant.h"
#include "expressions/conditional_assembly/terms/ca_expr_list.h"
#include "expressions/conditional_assembly/terms/ca_function.h"
#include "expressions/conditional_assembly/terms/ca_string.h"
#include "expressions/conditional_assembly/terms/ca_symbol.h"
#include "expressions/conditional_assembly/terms/ca_symbol_attribute.h"
#include "expressions/conditional_assembly/terms/ca_var_sym.h"
#include "expressions/mach_expr_term.h"
#include "expressions/mach_expr_visitor.h"
#include "expressions/mach_expression.h"
#include "expressions/nominal_value.h"
#include "lexing/string_with_newlines.h"
#include "processing/op_code.h"
#include "semantics/operand.h"
#include "semantics/operand_impls.h"
#include "utils/scope_exit.h"
#include "utils/string_operations.h"
#include "utils/truth_table.h"
#include "utils/unicode_text.h"

namespace hlasm_plugin::parser_library::parsing {

template<bool multiline>
struct parser_holder_impl final : parser_holder
{
    parser_holder_impl(context::hlasm_context* hl_ctx, diagnostic_op_consumer* d)
    {
        hlasm_ctx = hl_ctx;
        diagnostic_collector = d;
    }
};

std::unique_ptr<parser_holder> parser_holder::create(
    context::hlasm_context* hl_ctx, diagnostic_op_consumer* d, bool multiline)
{
    if (multiline)
        return std::make_unique<parser_holder_impl<true>>(hl_ctx, d);
    else
        return std::make_unique<parser_holder_impl<false>>(hl_ctx, d);
}

parser_holder::~parser_holder() = default;

void parser_holder::prepare_parser(lexing::u8string_view_with_newlines text,
    context::hlasm_context* hl_ctx,
    diagnostic_op_consumer* diags,
    semantics::range_provider rp,
    range text_range,
    size_t logical_column,
    const processing::processing_status& ps)
{
    hlasm_ctx = hl_ctx;
    diagnostic_collector = diags;
    range_prov = std::move(rp);
    proc_status = ps;

    reset(text, text_range.start, logical_column);

    collector.prepare_for_next_statement();
}

constexpr const auto EOF_SYMBOL = (parser_holder::char_t)-1;
template<parser_holder::char_t... chars>
requires((chars != EOF_SYMBOL) && ...) struct group_t
{
    [[nodiscard]] static constexpr bool matches(parser_holder::char_t ch) noexcept { return ((ch == chars) || ...); }
};
template<parser_holder::char_t... chars>
constexpr group_t<chars...> group = {};

template<std::array<parser_holder::char_t, 256> s>
constexpr auto group_from_string()
{
    constexpr auto n = std::ranges::find(s, U'\0') - s.begin();
    return []<size_t... i>(std::index_sequence<i...>) {
        return group_t<s[i]...>(); //
    }(std::make_index_sequence<n>());
}

constexpr auto selfdef = group_from_string<{ U"BXCGbxcg" }>();
constexpr auto mach_attrs = group_from_string<{ U"OSILTosilt" }>();
constexpr auto all_attrs = group_from_string<{ U"NKDOSILTnkdosilt" }>();
constexpr auto attr_argument = group_from_string<{ U"$_#@abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ&=*" }>();

constexpr const auto ord_first = utils::create_truth_table("$_#@abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
constexpr const auto ord =
    utils::create_truth_table("$_#@abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
constexpr const auto numbers = utils::create_truth_table("0123456789");
constexpr const auto identifier_divider = utils::create_truth_table("*.-+=<>,()'/&| ");

[[nodiscard]] constexpr bool char_is_ord_first(parser_holder::char_t c) noexcept
{
    return c < ord_first.size() && ord_first[c];
}
[[nodiscard]] constexpr bool char_is_ord(parser_holder::char_t c) noexcept { return c < ord.size() && ord[c]; }
[[nodiscard]] constexpr bool char_is_num(parser_holder::char_t c) noexcept { return c < numbers.size() && numbers[c]; }

namespace {
std::pair<char_substitution, size_t> append_utf8_with_newlines_to_utf32(
    std::vector<parser_holder::char_t>& t, std::vector<size_t>& nl, std::vector<size_t>& ll, std::string_view s)
{
    char_substitution subs {};

    size_t utf16_length = 0;
    while (!s.empty())
    {
        unsigned char c = s.front();
        if (c < 0x80)
        {
            t.push_back(c);
            s.remove_prefix(1);
            ++utf16_length;
            continue;
        }
        else if (c == lexing::u8string_view_with_newlines::EOLuc)
        {
            nl.push_back(t.size());
            ll.push_back(std::exchange(utf16_length, 0));
            s.remove_prefix(1);
            continue;
        }
        const auto cs = utils::utf8_prefix_sizes[c];
        if (cs.utf8 && cs.utf8 <= s.size())
        {
            char32_t v = c & 0b0111'1111u >> cs.utf8;
            for (int i = 1; i < cs.utf8; ++i)
                v = v << 6 | (s[i] & 0b0011'1111u);

            if (v == utils::substitute_character)
                subs.client = true;

            t.push_back(v);
            s.remove_prefix(cs.utf8);
            utf16_length += cs.utf16;
        }
        else
        {
            subs.server = true;
            t.push_back(utils::substitute_character);
            s.remove_prefix(1);
            ++utf16_length;
        }
    }

    return { subs, utf16_length };
}
} // namespace

void parser_holder::reset(position file_offset, size_t logical_column, bool process)
{
    process_allowed = process;

    input.push_back(EOF_SYMBOL);
    newlines.push_back((size_t)-1);
    input_state.next = input.data();
    input_state.nl = newlines.data();
    input_state.line = file_offset.line;
    input_state.char_position_in_line = logical_column;
    input_state.char_position_in_line_utf16 = file_offset.column;
    input_state.last = &input.back();

    for (auto bump_line = file_offset.column; auto& ll : line_limits)
    {
        ll += bump_line;
        bump_line = cont;
    }
}

char_substitution parser_holder::reset(
    lexing::u8string_view_with_newlines str, position file_offset, size_t logical_column, bool process)
{
    input.clear();
    newlines.clear();
    line_limits.clear();
    auto [subs, _] = append_utf8_with_newlines_to_utf32(input, newlines, line_limits, str.text);

    reset(file_offset, logical_column, process);

    return subs;
}

char_substitution parser_holder::reset(
    const lexing::logical_line<utils::utf8_iterator<std::string_view::iterator, utils::utf8_utf16_counter>>& l,
    position file_offset,
    size_t logical_column,
    bool process)
{
    char_substitution subs {};

    input.clear();
    newlines.clear();
    line_limits.clear();

    for (size_t i = 0; i < l.segments.size(); ++i)
    {
        const auto& s = l.segments[i];

        auto [subs_update, utf16_rem] = append_utf8_with_newlines_to_utf32(
            input, newlines, line_limits, std::string_view(s.code_begin().base(), s.code_end().base()));

        subs |= subs_update;

        if (i + 1 < l.segments.size())
        {
            newlines.push_back(input.size());
            line_limits.push_back(utf16_rem);
        }
    }

    reset(file_offset, logical_column, process);

    return subs;
}

struct parser_holder::parser2
{
    parser_holder* holder;

    input_state_t input;
    const lexing::char_t* data;

    std::vector<range> remarks;

    bool ca_string_enabled = true;
    bool literals_allowed = true;

    ///////////////////////////////////////////////////////////////////

    bool allow_ca_string() const noexcept { return ca_string_enabled; }
    void enable_ca_string() noexcept { ca_string_enabled = true; }
    void disable_ca_string() noexcept { ca_string_enabled = false; }

    class [[nodiscard]] literal_controller
    {
        enum class request_t
        {
            none,
            off,
            on,
        } request = request_t::none;
        parser2& impl;

    public:
        explicit literal_controller(parser2& impl) noexcept
            : impl(impl)
        {}
        literal_controller(parser2& impl, bool restore) noexcept
            : request(restore ? request_t::on : request_t::off)
            , impl(impl)
        {}
        literal_controller(literal_controller&& oth) noexcept
            : request(std::exchange(oth.request, request_t::none))
            , impl(oth.impl)
        {}
        ~literal_controller()
        {
            switch (request)
            {
                case request_t::off:
                    impl.literals_allowed = false;
                    break;
                case request_t::on:
                    impl.literals_allowed = true;
                    break;
                default:
                    break;
            }
        }
    };

    bool allow_literals() const noexcept { return literals_allowed; }
    literal_controller enable_literals() noexcept
    {
        if (literals_allowed)
            return literal_controller(*this);

        literals_allowed = true;
        return literal_controller(*this, false);
    }
    literal_controller disable_literals() noexcept
    {
        if (!literals_allowed)
            return literal_controller(*this);

        literals_allowed = false;
        return literal_controller(*this, true);
    }


    [[nodiscard]] constexpr bool before_nl() const noexcept
    {
        return static_cast<size_t>(input.next - data) < *input.nl;
    }

    constexpr void adjust_lines() noexcept
    {
        if (before_nl())
            return;

        input.char_position_in_line = holder->cont;
        input.char_position_in_line_utf16 = holder->cont;
        do
        {
            ++input.line;
            ++input.nl;
        } while (!before_nl());
        return;
    }

    void consume() noexcept
    {
        assert(!eof());

        const auto ch = *input.next;

        adjust_lines();

        ++input.next;
        ++input.char_position_in_line;
        input.char_position_in_line_utf16 += 1 + (ch > 0xffffu);
    }

    void consume(hl_scopes s) noexcept
    {
        assert(!eof());

        const auto pos = cur_pos_adjusted();
        consume();
        const auto end = cur_pos();

        add_hl_symbol({ pos, end }, s);
    }

    void consume_into(std::string& s)
    {
        assert(!eof());
        utils::append_utf32_to_utf8(s, *input.next);
        consume();
    }

    void consume_into(std::string& s, hl_scopes scope)
    {
        assert(!eof());
        utils::append_utf32_to_utf8(s, *input.next);
        consume(scope);
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

    [[nodiscard]] range remap_range(range r) const noexcept { return holder->range_prov.adjust_range(r); }

    void add_diagnostic(diagnostic_op d)
    {
        if (holder->diagnostic_collector)
            holder->diagnostic_collector->add_diagnostic(std::move(d));
    }

    void add_diagnostic(diagnostic_op (&d)(const range&))
    {
        add_diagnostic(d(holder->range_prov.adjust_range(range(cur_pos()))));
    }

    void syntax_error_or_eof()
    {
        if (*input.next == EOF_SYMBOL)
            add_diagnostic(diagnostic_op::error_S0003);
        else
            add_diagnostic(diagnostic_op::error_S0002);
    }

    void add_hl_symbol(const range& r, hl_scopes s) { add_hl_symbol_remapped(remap_range(r), s); }

    void add_hl_symbol_remapped(const range& r, hl_scopes s) { holder->collector.add_hl_symbol(token_info(r, s)); }

    context::id_index parse_identifier(std::string value, range id_range)
    {
        if (value.size() > 63 && holder->diagnostic_collector)
            holder->diagnostic_collector->add_diagnostic(diagnostic_op::error_S100(value, id_range));

        return holder->hlasm_ctx->ids().add(std::move(value));
    }

    // TODO: This should be changed, so the id_index is always valid ordinary symbol
    context::id_index add_id(std::string value) { return holder->hlasm_ctx->ids().add(std::move(value)); }
    context::id_index add_id(std::string_view value) { return holder->hlasm_ctx->ids().add(value); }

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
            remarks.push_back(remap_range({ last_remark_start, last_remark_end }));
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
                remarks.push_back(remap_range({ last_remark_start, remark_end }));
        }
    }

    [[nodiscard]] constexpr bool is_ord_first() const noexcept { return char_is_ord_first(*input.next); }
    [[nodiscard]] constexpr bool is_ord() const noexcept { return char_is_ord(*input.next); }
    [[nodiscard]] constexpr bool is_num() const noexcept { return char_is_num(*input.next); }

    [[nodiscard]] constexpr bool eof() const noexcept { return *input.next == EOF_SYMBOL; }

    template<parser_holder::char_t... chars>
    [[nodiscard]] constexpr bool except() const noexcept requires((chars != EOF_SYMBOL) && ...)
    {
        const auto ch = *input.next;
        return ((ch != EOF_SYMBOL) && ... && (ch != chars));
    }

    template<auto... groups>
    [[nodiscard]] constexpr bool follows() const noexcept requires(((&decltype(groups)::matches, true) && ...))
    {
        return [this]<size_t... idx>(std::index_sequence<idx...>) {
            return (decltype(groups)::matches(input.next[idx]) && ...); //
        }(std::make_index_sequence<sizeof...(groups)>());
    }

    template<parser_holder::char_t... chars>
    [[nodiscard]] constexpr bool follows() const noexcept requires((chars != EOF_SYMBOL) && ...)
    {
        const auto ch = *input.next;
        return ((ch == chars) || ...);
    }

    template<parser_holder::char_t... chars>
    [[nodiscard]] constexpr bool must_follow() requires((chars != EOF_SYMBOL) && ...)
    {
        if (follows<chars...>())
            return true;

        syntax_error_or_eof();

        return false;
    }

    template<parser_holder::char_t... chars>
    [[nodiscard]] constexpr bool must_follow(diagnostic_op (&d)(const range&)) requires((chars != EOF_SYMBOL) && ...)
    {
        if (follows<chars...>())
            return true;

        add_diagnostic(d);
        return false;
    }

    template<parser_holder::char_t... chars>
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

    template<parser_holder::char_t... chars>
    [[nodiscard]] constexpr bool match(hl_scopes s, diagnostic_op (&d)(const range&))
        requires((chars != EOF_SYMBOL) && ...)
    {
        if (!follows<chars...>())
        {
            add_diagnostic(d);
            return false;
        }
        consume(s);
        return true;
    }

    template<parser_holder::char_t... chars>
    [[nodiscard]] constexpr bool match() requires((chars != EOF_SYMBOL) && ...)
    {
        if (must_follow<chars...>())
        {
            consume();
            return true;
        }
        syntax_error_or_eof();
        return false;
    }

    template<parser_holder::char_t... chars>
    [[nodiscard]] constexpr bool match(hl_scopes s) requires((chars != EOF_SYMBOL) && ...)
    {
        if (must_follow<chars...>())
        {
            consume(s);
            return true;
        }
        return false;
    }

    template<parser_holder::char_t... chars>
    [[nodiscard]] constexpr bool try_consume() requires((chars != EOF_SYMBOL) && ...)
    {
        if (follows<chars...>())
        {
            consume();
            return true;
        }
        return false;
    }

    template<parser_holder::char_t... chars>
    [[nodiscard]] constexpr bool try_consume(hl_scopes s) requires((chars != EOF_SYMBOL) && ...)
    {
        if (follows<chars...>())
        {
            consume(s);
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

    void resolve_expression(expressions::ca_expr_ptr& expr) const
    {
        diagnostic_consumer_transform diags([diags = holder->diagnostic_collector](diagnostic_op d) {
            if (diags)
                diags->add_diagnostic(std::move(d));
        });
        using enum context::SET_t_enum;
        auto [_, opcode] = *holder->proc_status;
        using wk = context::id_storage::well_known;
        if (opcode.value == wk::SETA || opcode.value == wk::ACTR || opcode.value == wk::ASPACE
            || opcode.value == wk::AGO || opcode.value == wk::MHELP)
            expr->resolve_expression_tree({ A_TYPE, A_TYPE, true }, diags);
        else if (opcode.value == wk::SETB)
        {
            if (!expr->is_compatible(expressions::ca_expression_compatibility::setb))
                diags.add_diagnostic(diagnostic_op::error_CE016_logical_expression_parenthesis(expr->expr_range));

            expr->resolve_expression_tree({ B_TYPE, B_TYPE, true }, diags);
        }
        else if (opcode.value == wk::AIF)
        {
            if (!expr->is_compatible(expressions::ca_expression_compatibility::aif))
                diags.add_diagnostic(diagnostic_op::error_CE016_logical_expression_parenthesis(expr->expr_range));

            expr->resolve_expression_tree({ B_TYPE, B_TYPE, true }, diags);
        }
        else if (opcode.value == wk::SETC)
        {
            expr->resolve_expression_tree({ C_TYPE, C_TYPE, true }, diags);
        }
        else if (opcode.value == wk::AREAD)
        {
            // aread operand is just enumeration
        }
        else
        {
            assert(false);
            expr->resolve_expression_tree({ UNDEF_TYPE, UNDEF_TYPE, true }, diags);
        }
    }

    void resolve_concat_chain(const semantics::concat_chain& chain) const
    {
        diagnostic_consumer_transform diags([diags = holder->diagnostic_collector](diagnostic_op d) {
            if (diags)
                diags->add_diagnostic(std::move(d));
        });
        for (const auto& e : chain)
            e.resolve(diags);
    }

    std::string lex_ord()
    {
        assert(is_ord_first());

        std::string result;
        do
        {
            consume_into(result);
        } while (is_ord());

        return result;
    }

    std::string lex_ord_upper()
    {
        assert(is_ord_first());

        std::string result;
        do
        {
            consume_into(result);
        } while (is_ord());

        utils::to_upper(result);

        return result;
    }

    result_t<context::id_index> lex_id()
    {
        assert(is_ord_first());

        const auto start = cur_pos_adjusted();

        std::string name = lex_ord();

        const auto end = cur_pos();

        auto id = parse_identifier(std::move(name), { start, end });
        if (id.empty())
            return failure;
        else
            return id;
    }

    struct qualified_id
    {
        context::id_index qual;
        context::id_index id;
    };
    result_t<qualified_id> lex_qualified_id()
    {
        auto [error, id1] = lex_id();
        if (error)
            return failure;

        if (try_consume<U'.'>(hl_scopes::operator_symbol))
        {
            if (!is_ord_first())
            {
                syntax_error_or_eof();
                return failure;
            }

            auto [error2, id2] = lex_id();
            if (error2)
                return failure;

            return { id1, id2 };
        }

        return { context::id_index(), id1 };
    }

    result_t<std::variant<context::id_index, semantics::concat_chain>> lex_variable_name(position start);
    result_t<semantics::vs_ptr> lex_variable();

    result_t<semantics::concat_chain> lex_compound_variable()
    {
        if (!except<U')'>())
        {
            syntax_error_or_eof();
            return failure;
        }
        semantics::concat_chain result;

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
                    result.emplace_back(std::in_place_type<semantics::var_sym_conc>, std::move(var));
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
                    consume(hl_scopes::operator_symbol);
                    result.emplace_back(std::in_place_type<semantics::dot_conc>, remap_range({ start, cur_pos() }));
                    break;
                }

                default: {
                    const auto start = cur_pos_adjusted();
                    std::string collected;

                    while (except<U')', U'&', U'.'>())
                    {
                        consume_into(collected);
                    }
                    const auto r = remap_range({ start, cur_pos() });
                    result.emplace_back(std::in_place_type<semantics::char_str_conc>, std::move(collected), r);
                    add_hl_symbol_remapped(r, hl_scopes::var_symbol);
                    break;
                }
            }
        }
        add_diagnostic(diagnostic_op::error_S0011);
        return failure;
    }

    [[nodiscard]] constexpr bool follows_NOT() const noexcept
    {
        return follows<group<U'N', U'n'>, group<U'O', U'o'>, group<U'T', U't'>>() && input.next[3] != EOF_SYMBOL
            && !char_is_ord(input.next[3]);
    }

    static constexpr auto PROCESS = context::id_index("*PROCESS");
    [[nodiscard]] constexpr bool follows_PROCESS() const noexcept
    {
        return follows<group<U'*'>,
                   group<U'P', U'p'>,
                   group<U'R', U'r'>,
                   group<U'O', U'o'>,
                   group<U'C', U'c'>,
                   group<U'E', U'e'>,
                   group<U'S', U's'>,
                   group<U'S', U's'>>()
            && (input.next[PROCESS.size()] == EOF_SYMBOL || input.next[PROCESS.size()] == U' ');
    }

    result_t<semantics::seq_sym> lex_seq_symbol()
    {
        const auto start = cur_pos_adjusted();
        if (!try_consume<U'.'>() || !is_ord_first())
        {
            syntax_error_or_eof();
            return failure;
        }
        auto [error, id] = lex_id();
        if (error)
            return failure;
        const auto r = remap_range({ start, cur_pos() });
        add_hl_symbol_remapped(r, hl_scopes::seq_symbol);
        return { id, r };
    }

    result_t<expressions::ca_expr_ptr> lex_expr_general()
    {
        const auto start = cur_pos_adjusted();
        if (!follows_NOT())
            return lex_expr();

        std::vector<expressions::ca_expr_ptr> ca_exprs;
        do
        {
            const auto start_not = cur_pos_adjusted();
            consume();
            consume();
            consume();
            const auto r = remap_range({ start_not, cur_pos() });
            add_hl_symbol_remapped(r, hl_scopes::operand);
            ca_exprs.push_back(std::make_unique<expressions::ca_symbol>(context::id_index("NOT"), r));
            lex_optional_space();
        } while (follows_NOT());

        auto [error, e] = lex_expr();
        if (error)
            return failure;

        ca_exprs.push_back(std::move(e));
        return std::make_unique<expressions::ca_expr_list>(
            std::move(ca_exprs), remap_range({ start, cur_pos() }), false);
    }

    result_t<semantics::concat_chain> lex_ca_string_value()
    {
        assert(follows<U'\''>());

        auto start = cur_pos_adjusted();

        semantics::concat_chain cc;

        std::string s;
        const auto dump_s = [&]() {
            if (s.empty())
                return;
            cc.emplace_back(
                std::in_place_type<semantics::char_str_conc>, std::move(s), remap_range({ start, cur_pos() }));
            s.clear();
        };

        consume();
        while (true)
        {
            switch (*input.next)
            {
                case EOF_SYMBOL:
                    add_diagnostic(diagnostic_op::error_S0005);
                    return failure;

                case U'.':
                    dump_s();
                    start = cur_pos_adjusted();
                    consume();
                    cc.emplace_back(std::in_place_type<semantics::dot_conc>, remap_range({ start, cur_pos() }));
                    start = cur_pos_adjusted();
                    break;

                case U'=':
                    dump_s();
                    start = cur_pos_adjusted();
                    consume();
                    cc.emplace_back(std::in_place_type<semantics::equals_conc>, remap_range({ start, cur_pos() }));
                    start = cur_pos_adjusted();
                    break;

                case U'&':
                    if (input.next[1] == U'&')
                    {
                        consume_into(s);
                        consume_into(s);
                    }
                    else
                    {
                        dump_s();
                        auto [error, vs] = lex_variable();
                        if (error)
                            return failure;
                        cc.emplace_back(std::in_place_type<semantics::var_sym_conc>, std::move(vs));
                        start = cur_pos_adjusted();
                    }
                    break;

                case U'\'':
                    consume();
                    if (!follows<U'\''>())
                    {
                        dump_s();
                        semantics::concatenation_point::clear_concat_chain(cc);
                        return cc;
                    }
                    consume_into(s);
                    break;

                default:
                    consume_into(s);
                    break;
            }
        }
    }

    result_t<expressions::ca_string::substring_t> lex_substring()
    {
        assert(follows<U'('>());

        const auto sub_start = cur_pos_adjusted();

        consume(hl_scopes::operator_symbol);

        auto [e1_error, e1] = lex_expr_general();
        if (e1_error)
            return failure;

        if (!match<U','>(hl_scopes::operator_symbol))
            return failure;

        if (try_consume<U'*'>()) // TODO: no hightlighting?
        {
            if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
                return failure;
            return { std::move(e1), expressions::ca_expr_ptr(), remap_range({ sub_start, cur_pos() }) };
        }

        auto [e2_error, e2] = lex_expr_general();
        if (e2_error)
            return failure;

        if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
            return failure;

        return {
            std::move(e1),
            std::move(e2),
            remap_range({ sub_start, cur_pos() }),
        };
    }

    result_t<std::pair<semantics::concat_chain, expressions::ca_string::substring_t>>
    lex_ca_string_with_optional_substring()
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
        while (try_consume<U' '>())
        {
            matched = true;
        }
        return matched;
    }

    result_t<std::vector<expressions::ca_expr_ptr>> lex_subscript_ne()
    {
        assert(follows<U'('>());

        std::vector<expressions::ca_expr_ptr> result;

        consume(hl_scopes::operator_symbol);
        if (lex_optional_space())
        {
            auto [error, e] = lex_expr();
            if (error)
                return failure;

            result.push_back(std::move(e));
            lex_optional_space();
            if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
                return failure;

            return result;
        }

        if (auto [error, e] = lex_expr(); error)
            return failure;
        else
            result.push_back(std::move(e));

        if (lex_optional_space())
        {
            if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
                return failure;
            return result;
        }
        if (try_consume<U')'>(hl_scopes::operator_symbol))
            return result;

        if (!match<U','>(hl_scopes::operator_symbol, diagnostic_op::error_S0002))
            return failure;

        if (auto [error, e] = lex_expr(); error)
            return failure;
        else
            result.push_back(std::move(e));

        while (try_consume<U','>(hl_scopes::operator_symbol))
        {
            if (auto [error, e] = lex_expr(); error)
                return failure;
            else
                result.push_back(std::move(e));
        }
        if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
            return failure;

        return result;
    }

    [[nodiscard]] auto parse_self_def_term(std::string_view type, std::string_view value, range r)
    {
        auto add_diagnostic =
            holder->diagnostic_collector ? diagnostic_adder(*holder->diagnostic_collector, r) : diagnostic_adder(r);
        return expressions::ca_constant::self_defining_term(type, value, add_diagnostic);
    }
    [[nodiscard]] auto parse_self_def_term_in_mach(std::string_view type, std::string_view value, range r)
    {
        auto add_diagnostic =
            holder->diagnostic_collector ? diagnostic_adder(*holder->diagnostic_collector, r) : diagnostic_adder(r);
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

    result_t<expressions::ca_expr_ptr> lex_rest_of_ca_string_group(
        expressions::ca_expr_ptr initial_duplicate_factor, const position& start)
    {
        if (!allow_ca_string())
            return failure;
        auto [error, s] = lex_ca_string_with_optional_substring();
        if (error)
            return failure;

        expressions::ca_expr_ptr result = std::make_unique<expressions::ca_string>(std::move(s.first),
            std::move(initial_duplicate_factor),
            std::move(s.second),
            remap_range({ start, cur_pos() }));

        while (follows<U'(', U'\''>())
        {
            const auto conc_start = cur_pos_adjusted();
            expressions::ca_expr_ptr nested_dupl;
            if (try_consume<U'('>(hl_scopes::operator_symbol))
            {
                auto [error2, dupl] = lex_expr_general();
                if (error2)
                    return failure;
                if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
                    return failure;
                nested_dupl = std::move(dupl);
            }
            auto [error2, s2] = lex_ca_string_with_optional_substring();
            if (error2)
                return failure;

            result = std::make_unique<expressions::ca_basic_binary_operator<expressions::ca_conc>>(std::move(result),
                std::make_unique<expressions::ca_string>(std::move(s2.first),
                    std::move(nested_dupl),
                    std::move(s2.second),
                    remap_range({ conc_start, cur_pos() })),
                remap_range({ start, cur_pos() }));
        }
        return result;
    }

    struct maybe_expr_list
    {
        std::variant<std::vector<expressions::ca_expr_ptr>, expressions::ca_expr_ptr> value;
        bool leading_trailing_spaces;
    };

    result_t<maybe_expr_list> lex_maybe_expression_list()
    {
        expressions::ca_expr_ptr p_expr;
        std::vector<expressions::ca_expr_ptr> expr_list;

        auto leading_spaces = lex_optional_space();
        if (auto [error, e] = lex_expr(); error)
            return failure;
        else
            p_expr = std::move(e);

        auto trailing_spaces = lex_optional_space();
        for (; except<U')'>(); trailing_spaces = lex_optional_space())
        {
            auto [error, e] = lex_expr();
            if (error)
                return failure;
            if (p_expr)
                expr_list.push_back(std::move(p_expr));
            expr_list.push_back(std::move(e));
        }
        const auto lt_spaces = leading_spaces || trailing_spaces;
        if (lt_spaces && p_expr)
            expr_list.push_back(std::move(p_expr));
        if (!expr_list.empty())
            return { std::move(expr_list), lt_spaces };
        else
            return { std::move(p_expr), lt_spaces };
    }

    result_t<expressions::ca_expr_ptr> lex_expr_list()
    {
        assert(follows<U'('>());
        const auto start = cur_pos_adjusted();

        consume(hl_scopes::operator_symbol);

        std::vector<expressions::ca_expr_ptr> expr_list;

        (void)lex_optional_space();
        if (auto [error, e] = lex_expr(); error)
            return failure;
        else
            expr_list.push_back(std::move(e));

        (void)lex_optional_space();
        for (; except<U')'>(); (void)lex_optional_space())
        {
            auto [error, e] = lex_expr();
            if (error)
                return failure;
            expr_list.push_back(std::move(e));
        }
        if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
            return failure;
        return std::make_unique<expressions::ca_expr_list>(
            std::move(expr_list), remap_range({ start, cur_pos() }), true);
    }

    result_t<expressions::ca_expr_ptr> lex_self_def()
    {
        assert((follows<selfdef, group<U'\''>>()));
        const auto start = cur_pos_adjusted();

        const auto c = static_cast<char>(*input.next);
        consume(hl_scopes::self_def_type);
        auto [error, s] = lex_simple_string();
        if (error)
            return failure;

        const auto r = remap_range({ start, cur_pos() });
        return std::make_unique<expressions::ca_constant>(
            parse_self_def_term(std::string_view(&c, 1), std::move(s), r), r);
    }

    result_t<expressions::ca_expr_ptr> lex_attribute_reference()
    {
        assert((follows<all_attrs, group<U'\''>>()));
        const auto start = cur_pos_adjusted();

        const auto attr = context::symbol_attributes::transform_attr(utils::upper_cased[*input.next]);
        consume(hl_scopes::data_attr_type);
        consume(hl_scopes::operator_symbol);

        const auto start_value = cur_pos_adjusted();
        switch (*input.next)
        {
            case U'&': {
                auto [error, v] = lex_variable();
                if (error)
                    return failure;
                // TODO: in reality, this seems to be much more complicated (arbitrary many dots
                // are consumed for *some* attributes)
                // TODO: highlighting
                (void)try_consume<U'.'>();

                return std::make_unique<expressions::ca_symbol_attribute>(
                    std::move(v), attr, remap_range({ start, cur_pos() }), remap_range({ start_value, cur_pos() }));
            }

            case U'*':
                add_diagnostic(diagnostic_op::error_S0014);
                return failure;

            case U'=': {
                auto [error, l] = lex_literal();
                if (error)
                    return failure;
                return std::make_unique<expressions::ca_symbol_attribute>(
                    std::move(l), attr, remap_range({ start, cur_pos() }), remap_range({ start_value, cur_pos() }));
            }

            default: {
                if (!is_ord_first())
                {
                    syntax_error_or_eof();
                    return failure;
                }
                const auto id_start = cur_pos_adjusted();
                auto [error, id] = lex_id();
                if (error)
                    return failure;
                const auto id_r = remap_range({ id_start, cur_pos() });
                add_hl_symbol_remapped(id_r, hl_scopes::ordinary_symbol);
                return std::make_unique<expressions::ca_symbol_attribute>(
                    id, attr, remap_range({ start, cur_pos() }), id_r);
            }
        }
    }

    bool follows_function()
    {
        if (!is_ord_first())
            return false;
        const auto* p = input.next;
        std::string s;
        while (char_is_ord(*p))
        {
            if (s.size() >= expressions::ca_common_expr_policy::max_function_name_length)
                return false;
            s.push_back((char)*p);
            ++p;
        }
        return expressions::ca_common_expr_policy::get_function(s) != expressions::ca_expr_funcs::UNKNOWN;
    }

    result_t<expressions::ca_expr_ptr> lex_term()
    {
        const auto start = cur_pos_adjusted();
        switch (*input.next)
        {
            case U'&': {
                auto [error, v] = lex_variable();
                if (error)
                    return failure;
                return std::make_unique<expressions::ca_var_sym>(std::move(v), remap_range({ start, cur_pos() }));
            }

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
            case U'9':
                return lex_num();

            case U'\'':
                if (auto [error, s] = lex_rest_of_ca_string_group({}, start); error)
                    return failure;
                else
                {
                    add_hl_symbol({ start, cur_pos() }, hl_scopes::string);
                    return std::move(s);
                }

            case U'(': {
                consume(hl_scopes::operator_symbol);

                auto [error, maybe_expr_list] = lex_maybe_expression_list();
                if (error)
                    return failure;
                if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
                    return failure;
                expressions::ca_expr_ptr p_expr;
                const auto already_expr_list =
                    std::holds_alternative<std::vector<expressions::ca_expr_ptr>>(maybe_expr_list.value);
                if (already_expr_list)
                    p_expr = std::make_unique<expressions::ca_expr_list>(
                        std::move(std::get<std::vector<expressions::ca_expr_ptr>>(maybe_expr_list.value)),
                        remap_range({ start, cur_pos() }),
                        true);
                else
                    p_expr = std::move(std::get<expressions::ca_expr_ptr>(maybe_expr_list.value));

                if (maybe_expr_list.leading_trailing_spaces)
                    return p_expr;

                if (follows<U'\''>())
                {
                    if (auto [error2, s] = lex_rest_of_ca_string_group(std::move(p_expr), start); error2)
                        return failure;
                    else
                    {
                        add_hl_symbol({ start, cur_pos() }, hl_scopes::string);
                        return std::move(s);
                    }
                }
                else if (follows_function())
                {
                    auto [id_error, id] = lex_id();
                    if (id_error)
                        return failure;
                    if (!must_follow<U'('>())
                        return failure;
                    auto [s_error, s] = lex_subscript_ne();
                    if (s_error)
                        return failure;
                    return std::make_unique<expressions::ca_function>(id,
                        expressions::ca_common_expr_policy::get_function(id.to_string_view()),
                        std::move(s),
                        std::move(p_expr),
                        remap_range({ start, cur_pos() }));
                }

                if (!already_expr_list)
                {
                    std::vector<expressions::ca_expr_ptr> ops;
                    ops.push_back(std::move(p_expr));
                    p_expr = std::make_unique<expressions::ca_expr_list>(
                        std::move(ops), remap_range({ start, cur_pos() }), true);
                }

                return p_expr;
            }

            default:
                if (!is_ord_first())
                {
                    syntax_error_or_eof();
                    return failure;
                }

                if (follows<selfdef, group<U'\''>>())
                {
                    auto [error, self_def] = lex_self_def();
                    if (error)
                        return failure;
                    return std::move(self_def);
                }


                if (follows<all_attrs, group<U'\''>>())
                {
                    auto [error, attr_ref] = lex_attribute_reference();
                    if (error)
                        return failure;
                    return std::move(attr_ref);
                }

                if (auto [error, id] = lex_id(); error)
                    return failure;
                else if (follows<U'('>()
                    && expressions::ca_common_expr_policy::get_function(id.to_string_view())
                        != expressions::ca_expr_funcs::UNKNOWN)
                {
                    const auto r = remap_range({ start, cur_pos() });
                    add_hl_symbol_remapped(r, hl_scopes::operand);
                    auto [error2, s] = lex_subscript_ne();
                    if (error2)
                        return failure;
                    return std::make_unique<expressions::ca_function>(id,
                        expressions::ca_common_expr_policy::get_function(id.to_string_view()),
                        std::move(s),
                        expressions::ca_expr_ptr(),
                        remap_range({ start, cur_pos() }));
                }
                else
                {
                    const auto r = remap_range({ start, cur_pos() });
                    add_hl_symbol_remapped(r, hl_scopes::operand);
                    return std::make_unique<expressions::ca_symbol>(id, r);
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
            syntax_error_or_eof();
            return failure;
        }
        do
        {
            consume_into(result);
        } while (is_num());

        const auto r = remap_range({ start, cur_pos() });
        add_hl_symbol_remapped(r, hl_scopes::number);

        return { result, r };
    }

    result_t<std::pair<int32_t, range>> lex_number_as_int()
    {
        const auto [error, number] = lex_number_as_string();
        if (error)
            return failure;
        const auto& [v, r] = number;
        return { parse_self_def_term("D", v, r), r };
    }

    result_t<expressions::ca_expr_ptr> lex_num()
    {
        assert((follows<U'0', U'1', U'2', U'3', U'4', U'5', U'6', U'7', U'8', U'9', U'-'>()));
        const auto [error, number] = lex_number_as_int();
        if (error)
            return failure;
        const auto& [v, r] = number;
        return std::make_unique<expressions::ca_constant>(v, r);
    }


    result_t<expressions::mach_expr_ptr> lex_mach_term()
    {
        const auto start = cur_pos_adjusted();
        switch (*input.next)
        {
            case EOF_SYMBOL:
                add_diagnostic(diagnostic_op::error_S0003);
                return failure;

            case U'(': {
                consume(hl_scopes::operator_symbol);
                auto [error, e] = lex_mach_expr();
                if (error)
                    return failure;
                if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
                    return failure;
                return std::make_unique<expressions::mach_expr_unary<expressions::par>>(
                    std::move(e), remap_range({ start, cur_pos() }));
            }

            case U'*':
                consume(hl_scopes::operand);
                return std::make_unique<expressions::mach_expr_location_counter>(remap_range({ start, cur_pos() }));

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
                return std::make_unique<expressions::mach_expr_constant>(parse_self_def_term_in_mach("D", v, r), r);
            }

            case U'=': {
                auto [error, l] = lex_literal();
                if (error)
                    return failure;
                return std::make_unique<expressions::mach_expr_literal>(
                    remap_range({ start, cur_pos() }), std::move(l));
            }

            default:
                if (!is_ord_first())
                {
                    syntax_error_or_eof();
                    return failure;
                }
                if (follows<group<U'L', U'l'>, group<U'\''>, group<U'*'>>())
                {
                    consume(hl_scopes::data_attr_type);
                    consume(hl_scopes::operator_symbol);
                    auto loctr_len = maybe_loctr_len();
                    if (!loctr_len.has_value())
                    {
                        add_diagnostic(diagnostic_op::error_S0014);
                        return failure;
                    }
                    consume(hl_scopes::operand);
                    return std::make_unique<expressions::mach_expr_constant>(
                        loctr_len.value(), remap_range({ start, cur_pos() }));
                }
                if (follows<mach_attrs, group<U'\''>>())
                {
                    const auto attr = context::symbol_attributes::transform_attr(utils::upper_cased[*input.next]);
                    consume(hl_scopes::data_attr_type);
                    consume(hl_scopes::operator_symbol);
                    const auto start_value = cur_pos_adjusted();
                    if (follows<U'='>())
                    {
                        auto lit = enable_literals();
                        auto [error, l] = lex_literal();
                        if (error)
                            return failure;

                        return std::make_unique<expressions::mach_expr_data_attr_literal>(
                            std::make_unique<expressions::mach_expr_literal>(
                                remap_range({ start_value, cur_pos() }), std::move(l)),
                            attr,
                            remap_range({ start, cur_pos() }),
                            remap_range({ start_value, cur_pos() }));
                    }
                    if (is_ord_first())
                    {
                        auto [error, q_id] = lex_qualified_id();
                        if (error)
                            return failure;
                        add_hl_symbol({ start_value, cur_pos() }, hl_scopes::ordinary_symbol);
                        return std::make_unique<expressions::mach_expr_data_attr>(q_id.id,
                            q_id.qual,
                            attr,
                            remap_range({ start, cur_pos() }),
                            remap_range({ start_value, cur_pos() }));
                    }
                    syntax_error_or_eof();
                    return failure;
                }
                if (follows<group<U'C', U'c'>, group<U'A', U'E', U'U', U'a', U'e', U'u'>, group<U'\''>>())
                {
                    const char opt[2] = { static_cast<char>(input.next[0]), static_cast<char>(input.next[1]) };
                    consume();
                    consume();
                    add_hl_symbol({ start, cur_pos() }, hl_scopes::self_def_type);
                    auto [error, s] = lex_simple_string();
                    if (error)
                        return failure;

                    const auto r = remap_range({ start, cur_pos() });
                    return std::make_unique<expressions::mach_expr_constant>(
                        parse_self_def_term_in_mach(std::string_view(opt, 2), s, r), r);
                }
                if (follows<selfdef, group<U'\''>>())
                {
                    const auto opt = static_cast<char>(*input.next);
                    consume(hl_scopes::self_def_type);
                    auto [error, s] = lex_simple_string();
                    if (error)
                        return failure;

                    const auto r = remap_range({ start, cur_pos() });
                    return std::make_unique<expressions::mach_expr_constant>(
                        parse_self_def_term_in_mach(std::string_view(&opt, 1), s, r), r);
                }
                if (auto [error, qual_id] = lex_qualified_id(); error)
                    return failure;
                else
                {
                    const auto r = remap_range({ start, cur_pos() });
                    add_hl_symbol_remapped(r, hl_scopes::ordinary_symbol);
                    return std::make_unique<expressions::mach_expr_symbol>(qual_id.id, qual_id.qual, r);
                }
                break;
        }
    }

    result_t<std::string> lex_simple_string()
    {
        assert(follows<U'\''>());

        const auto start = cur_pos_adjusted();
        std::string s;

        consume();

        while (!eof())
        {
            if (follows<group<U'\''>, group<U'\''>>())
            {
                consume_into(s);
                consume();
            }
            else if (follows<U'\''>())
            {
                consume();
                add_hl_symbol({ start, cur_pos() }, hl_scopes::string);
                return s;
            }
            else if (follows<group<U'&'>, group<U'&'>>())
            {
                consume_into(s);
                consume();
            }
            else if (follows<U'&'>())
            {
                add_diagnostic(diagnostic_op::error_S0002);
                return failure;
            }
            else
            {
                consume_into(s);
            }
        }

        add_diagnostic(diagnostic_op::error_S0005);
        return failure;
    }

    result_t<expressions::mach_expr_ptr> lex_mach_term_c()
    {
        if (follows<U'+'>() || (follows<U'-'>() && !char_is_num(input.next[1])))
        {
            const auto plus = *input.next == U'+';
            const auto start = cur_pos_adjusted();
            consume(hl_scopes::operator_symbol);
            auto [error, e] = lex_mach_term_c();
            if (error)
                return failure;
            if (plus)
                return std::make_unique<expressions::mach_expr_unary<expressions::add>>(
                    std::move(e), remap_range({ start, cur_pos() }));
            else
                return std::make_unique<expressions::mach_expr_unary<expressions::sub>>(
                    std::move(e), remap_range({ start, cur_pos() }));
        }

        return lex_mach_term();
    }

    result_t<expressions::mach_expr_ptr> lex_mach_expr_s()
    {
        const auto start = cur_pos_adjusted();
        if (auto [error, e] = lex_mach_term_c(); error)
            return failure;
        else
        {
            while (follows<U'*', U'/'>())
            {
                const auto mul = *input.next == U'*';
                consume(hl_scopes::operator_symbol);
                auto [error2, next] = lex_mach_term_c();
                if (error2)
                    return failure;
                if (mul)
                    e = std::make_unique<expressions::mach_expr_binary<expressions::mul>>(
                        std::move(e), std::move(next), remap_range({ start, cur_pos() }));
                else
                    e = std::make_unique<expressions::mach_expr_binary<expressions::div>>(
                        std::move(e), std::move(next), remap_range({ start, cur_pos() }));
            }
            return std::move(e);
        }
    }

    result_t<expressions::mach_expr_ptr> lex_mach_expr()
    {
        const auto start = cur_pos_adjusted();
        if (auto [error, e] = lex_mach_expr_s(); error)
            return failure;
        else
        {
            while (follows<U'+', U'-'>())
            {
                const auto plus = *input.next == U'+';
                consume(hl_scopes::operator_symbol);
                auto [error2, next] = lex_mach_expr_s();
                if (error2)
                    return failure;
                if (plus)
                    e = std::make_unique<expressions::mach_expr_binary<expressions::add>>(
                        std::move(e), std::move(next), remap_range({ start, cur_pos() }));
                else
                    e = std::make_unique<expressions::mach_expr_binary<expressions::sub>>(
                        std::move(e), std::move(next), remap_range({ start, cur_pos() }));
            }
            return std::move(e);
        }
    }

    static bool is_type_extension(char type, char ch)
    {
        return checking::data_def_type::types_and_extensions.count(std::make_pair(type, ch)) > 0;
    }

    static constexpr int digit_to_value(parser_holder::char_t c)
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
        const auto r = remap_range({ start, cur_pos() });
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
        add_hl_symbol_remapped(r, hl_scopes::number);

        return { (int32_t)result, r };
    }

    result_t<expressions::mach_expr_ptr> lex_literal_signed_num()
    {
        if (try_consume<U'('>(hl_scopes::operator_symbol))
        {
            if (auto [error, e] = lex_mach_expr(); error)
                return failure;
            else if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
                return failure;
            else
                return std::move(e);
        }
        auto [error, n] = parse_number();
        if (error)
            return failure;
        return std::make_unique<expressions::mach_expr_constant>(n.first, n.second);
    }

    result_t<expressions::mach_expr_ptr> lex_literal_unsigned_num()
    {
        if (try_consume<U'('>(hl_scopes::operator_symbol))
        {
            auto [error, e] = lex_mach_expr();
            if (error)
                return failure;
            if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
                return failure;
            return std::move(e);
        }
        if (!is_num())
        {
            syntax_error_or_eof();
            return failure;
        }
        auto [error, n] = parse_number();
        if (error)
            return failure;
        return std::make_unique<expressions::mach_expr_constant>(n.first, n.second);
    }

    result_t<expressions::data_definition> lex_data_def_base()
    {
        const auto goff = holder->hlasm_ctx->goff();

        expressions::data_definition result;
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
            syntax_error_or_eof();
            return failure;
        }
        const auto type = utils::upper_cased[*input.next];
        const auto type_start = cur_pos_adjusted();
        consume();

        result.type = type == 'R' && !goff ? 'r' : type;
        result.type_range = remap_range({ type_start, cur_pos() });
        if (is_ord_first() && is_type_extension(type, utils::upper_cased[*input.next]))
        {
            result.extension = utils::upper_cased[*input.next];
            const auto ext_start = cur_pos_adjusted();
            consume();
            result.extension_range = remap_range({ ext_start, cur_pos() });
        }
        add_hl_symbol({ type_start, cur_pos() }, hl_scopes::data_def_type);

        // case state::try_reading_program:
        // case state::read_program:
        if (try_consume<U'P', U'p'>(hl_scopes::data_def_modifier))
        {
            auto [error, e] = lex_literal_signed_num();
            if (error)
                return failure;
            result.program_type = std::move(e);
        }
        // case state::try_reading_length:
        // case state::try_reading_bitfield:
        // case state::read_length:
        if (try_consume<U'L', U'l'>(hl_scopes::data_def_modifier))
        {
            if (try_consume<U'.'>())
            {
                result.length_type = expressions::data_definition::length_type::BIT;
            }
            auto [error, e] = lex_literal_unsigned_num();
            if (error)
                return failure;
            result.length = std::move(e);
        }

        // case state::try_reading_scale:
        // case state::read_scale:
        if (try_consume<U'S', U's'>(hl_scopes::data_def_modifier))
        {
            auto [error, e] = lex_literal_signed_num();
            if (error)
                return failure;
            result.scale = std::move(e);
        }

        // case state::try_reading_exponent:
        // case state::read_exponent:
        static constexpr auto can_have_exponent = group_from_string<{ U"DEFHLdefhl" }>();
        if (can_have_exponent.matches(result.type) && try_consume<U'E', U'e'>(hl_scopes::data_def_modifier))
        {
            auto [error, e] = lex_literal_signed_num();
            if (error)
                return failure;
            result.exponent = std::move(e);
        }
        return result;
    }

    result_t<expressions::expr_or_address> lex_expr_or_addr()
    {
        const auto start = cur_pos_adjusted();
        auto [error, e] = lex_mach_expr();
        if (error)
            return failure;

        if (!try_consume<U'('>(hl_scopes::operator_symbol))
            return { std::move(e) };
        auto [error2, e2] = lex_mach_expr();
        if (error2)
            return failure;
        if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
            return failure;
        return expressions::expr_or_address(std::in_place_type<expressions::address_nominal>,
            std::move(e),
            std::move(e2),
            remap_range({ start, cur_pos() }));
    }

    result_t<expressions::expr_or_address_list> lex_literal_nominal_addr()
    {
        assert(follows<U'('>());
        consume(hl_scopes::operator_symbol);

        expressions::expr_or_address_list result;

        auto [error, e] = lex_expr_or_addr();
        if (error)
            return failure;
        result.push_back(std::move(e));

        while (try_consume<U','>(hl_scopes::operator_symbol))
        {
            auto [error2, e_next] = lex_expr_or_addr();
            if (error2)
                return failure;
            result.push_back(std::move(e_next));
        }

        if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
            return failure;

        return result;
    }

    result_t<expressions::nominal_value_ptr> lex_literal_nominal()
    {
        const auto start = cur_pos_adjusted();
        if (follows<U'\''>())
        {
            auto [error, n] = lex_simple_string();
            if (error)
                return failure;
            return std::make_unique<expressions::nominal_value_string>(std::move(n), remap_range({ start, cur_pos() }));
        }
        else if (follows<U'('>())
        {
            auto [error, n] = lex_literal_nominal_addr();
            if (error)
                return failure;
            return std::make_unique<expressions::nominal_value_exprs>(std::move(n));
        }
        else
        {
            syntax_error_or_eof();
            return failure;
        }
    }

    result_t<expressions::data_definition> lex_data_definition(bool require_nominal)
    {
        auto [error, d] = lex_data_def_base();
        if (error)
            return failure;
        if (require_nominal || follows<U'(', U'\''>())
        {
            auto [error2, n] = lex_literal_nominal();
            if (error2)
                return failure;
            d.nominal_value = std::move(n);
        }

        struct loctr_reference_visitor final : public expressions::mach_expr_visitor
        {
            bool found_loctr_reference = false;

            void visit(const expressions::mach_expr_constant&) override {}
            void visit(const expressions::mach_expr_data_attr&) override {}
            void visit(const expressions::mach_expr_data_attr_literal&) override {}
            void visit(const expressions::mach_expr_symbol&) override {}
            void visit(const expressions::mach_expr_location_counter&) override { found_loctr_reference = true; }
            void visit(const expressions::mach_expr_default&) override {}
            void visit(const expressions::mach_expr_literal& expr) override { expr.get_data_definition().apply(*this); }
        } v;
        d.apply(v);
        d.references_loctr = v.found_loctr_reference;

        return std::move(d);
    }

    std::string capture_text(const parser_holder::char_t* start, const parser_holder::char_t* end) const
    {
        std::string s;
        s.reserve(end - start);
        std::for_each(start, end, [&s](auto c) { utils::append_utf32_to_utf8(s, c); });
        return s;
    }

    std::string capture_text(const parser_holder::char_t* start) const { return capture_text(start, input.next); }

    result_t<semantics::literal_si> lex_literal()
    {
        const auto allowed = allow_literals();
        const auto disabled = disable_literals();
        const auto start = cur_pos_adjusted();
        const auto initial = input.next;

        assert(follows<U'='>());
        consume(hl_scopes::operator_symbol);

        auto [error, dd] = lex_data_definition(true);
        if (error)
            return failure;

        if (!allowed)
        {
            add_diagnostic(diagnostic_op::error_S0013);
            // continue processing
        }

        return holder->collector.add_literal(capture_text(initial), std::move(dd), remap_range({ start, cur_pos() }));
    }

    result_t<expressions::ca_expr_ptr> lex_term_c()
    {
        if (follows<U'+'>() || (follows<U'-'>() && !char_is_num(input.next[1])))
        {
            const auto start = cur_pos_adjusted();
            const auto plus = *input.next == U'+';
            consume(hl_scopes::operator_symbol);
            auto [error, e] = lex_term_c();
            if (error)
                return failure;
            if (plus)
                return std::make_unique<expressions::ca_plus_operator>(std::move(e), remap_range({ start, cur_pos() }));
            else
                return std::make_unique<expressions::ca_minus_operator>(
                    std::move(e), remap_range({ start, cur_pos() }));
        }
        return lex_term();
    }

    result_t<expressions::ca_expr_ptr> lex_expr_s()
    {
        expressions::ca_expr_ptr result;
        const auto start = cur_pos_adjusted();
        auto [error, e] = lex_term_c();
        if (error)
            return failure;
        result = std::move(e);

        while (follows<U'*', U'/'>())
        {
            const auto mult = *input.next == U'*';
            consume(hl_scopes::operator_symbol);
            auto [error2, e_next] = lex_term_c();
            if (error2)
                return failure;
            if (mult)
                result = std::make_unique<expressions::ca_basic_binary_operator<expressions::ca_mul>>(
                    std::move(result), std::move(e_next), remap_range({ start, cur_pos() }));
            else
                result = std::make_unique<expressions::ca_basic_binary_operator<expressions::ca_div>>(
                    std::move(result), std::move(e_next), remap_range({ start, cur_pos() }));
        }

        return result;
    }

    result_t<expressions::ca_expr_ptr> lex_expr()
    {
        expressions::ca_expr_ptr result;
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
                    consume(hl_scopes::operator_symbol);
                    auto [error, e] = lex_expr_s();
                    if (error)
                        return failure;
                    if (plus)
                        result = std::make_unique<expressions::ca_basic_binary_operator<expressions::ca_add>>(
                            std::move(result), std::move(e), remap_range({ start, cur_pos() }));
                    else
                        result = std::make_unique<expressions::ca_basic_binary_operator<expressions::ca_sub>>(
                            std::move(result), std::move(e), remap_range({ start, cur_pos() }));
                }
                break;
            case '.':
                while (try_consume<U'.'>(hl_scopes::operator_symbol))
                {
                    auto [error, e] = lex_term_c();
                    if (error)
                        return failure;
                    result = std::make_unique<expressions::ca_basic_binary_operator<expressions::ca_conc>>(
                        std::move(result), std::move(e), remap_range({ start, cur_pos() }));
                }
                break;
        }

        return result;
    }

    result_t<std::vector<expressions::ca_expr_ptr>> lex_subscript()
    {
        assert(follows<U'('>());

        consume(hl_scopes::operator_symbol);

        std::vector<expressions::ca_expr_ptr> result;

        auto [error, expr] = lex_expr();
        if (error)
            return failure;
        result.push_back(std::move(expr));

        while (try_consume<U','>(hl_scopes::operator_symbol))
        {
            auto [error2, expr_next] = lex_expr();
            if (error2)
                return failure;
            result.push_back(std::move(expr_next));
        }

        if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
            return failure;

        return result;
    }

    class concat_chain_builder
    {
        parser2& p;
        semantics::concat_chain& cc;

        semantics::char_str_conc* last_text_state = nullptr;
        bool highlighting = true;

    public:
        concat_chain_builder(parser2& p, semantics::concat_chain& cc, bool hl = true) noexcept
            : p(p)
            , cc(cc)
            , highlighting(hl)
        {}

        [[nodiscard]] std::string& last_text_value()
        {
            if (last_text_state)
                return last_text_state->value;
            last_text_state = &std::get<semantics::char_str_conc>(
                cc.emplace_back(
                      std::in_place_type<semantics::char_str_conc>, std::string(), range(p.cur_pos_adjusted()))
                    .value);
            return last_text_state->value;
        }

        void push_last_text()
        {
            if (!last_text_state)
                return;
            last_text_state->conc_range = p.remap_range({ last_text_state->conc_range.start, p.cur_pos() });
            if (highlighting)
                p.add_hl_symbol_remapped(last_text_state->conc_range, hl_scopes::operand);
            last_text_state = nullptr;
        }

        template<typename T, hl_scopes... s>
        void single_char_push() requires(sizeof...(s) <= 1)
        {
            push_last_text();
            const auto start = p.cur_pos_adjusted();
            p.consume(s...);
            const auto r = p.remap_range({ start, p.cur_pos() });
            cc.emplace_back(std::in_place_type<T>, r);
            last_text_state = nullptr;
        }

        template<typename... Args>
        void emplace_back(Args&&... args)
        {
            push_last_text();
            cc.emplace_back(std::forward<Args>(args)...);
        }
    };

    result_t_void lex_macro_operand_amp(concat_chain_builder& ccb)
    {
        assert(follows<U'&'>());
        if (input.next[1] == U'&')
        {
            consume_into(ccb.last_text_value());
            consume_into(ccb.last_text_value());
        }
        else
        {
            ccb.push_last_text();
            auto [error, vs] = lex_variable();
            if (error)
                return failure;
            ccb.emplace_back(std::in_place_type<semantics::var_sym_conc>, std::move(vs));
        }
        return {};
    }

    result_t_void lex_macro_operand_string(concat_chain_builder& ccb)
    {
        assert(follows<U'\''>());

        consume_into(ccb.last_text_value());
        while (true)
        {
            switch (*input.next)
            {
                case EOF_SYMBOL:
                    ccb.push_last_text();
                    add_diagnostic(diagnostic_op::error_S0005);
                    return failure;
                case U'\'':
                    consume_into(ccb.last_text_value());
                    if (!follows<U'\''>())
                    {
                        ccb.push_last_text();
                        return {};
                    }
                    consume_into(ccb.last_text_value());
                    break;

                case U'&':
                    if (auto [error] = lex_macro_operand_amp(ccb); error)
                        return failure;
                    break;

                case U'=':
                    ccb.single_char_push<semantics::equals_conc>();
                    break;

                case U'.':
                    ccb.single_char_push<semantics::dot_conc>();
                    break;

                default:
                    consume_into(ccb.last_text_value());
                    break;
            }
        }
    }

    result_t<bool> lex_macro_operand_attr(concat_chain_builder& ccb)
    {
        if (input.next[1] != U'\'')
        {
            consume_into(ccb.last_text_value());
            return false;
        }

        if (char_is_ord_first(input.next[2]) || input.next[2] == U'=')
        {
            consume_into(ccb.last_text_value());
            consume_into(ccb.last_text_value());
            return false;
        }

        if (input.next[2] != U'&')
        {
            consume_into(ccb.last_text_value());
            return false;
        }

        while (except<U',', U')', U' '>())
        {
            if (!follows<U'&'>())
            {
                consume_into(ccb.last_text_value());
            }
            else if (input.next[1] == U'&')
            {
                consume_into(ccb.last_text_value());
                consume_into(ccb.last_text_value());
            }
            else if (auto [error, vs] = (ccb.push_last_text(), lex_variable()); error)
                return failure;
            else
            {
                ccb.emplace_back(std::in_place_type<semantics::var_sym_conc>, std::move(vs));
                if (follows<U'.'>())
                    ccb.single_char_push<semantics::dot_conc, hl_scopes::operator_symbol>();
            }
        }
        return true;
    }

    result_t_void lex_macro_operand(semantics::concat_chain& cc, bool next_char_special, bool op_name)
    {
        concat_chain_builder ccb(*this, cc);
        while (true)
        {
            const bool last_char_special = std::exchange(next_char_special, true);
            switch (*input.next)
            {
                case EOF_SYMBOL:
                case U' ':
                case U')':
                case U',':
                    ccb.push_last_text();
                    return {};

                case U'=':
                    ccb.single_char_push<semantics::equals_conc, hl_scopes::operator_symbol>();
                    next_char_special = false;
                    break;

                case U'.':
                    ccb.single_char_push<semantics::dot_conc, hl_scopes::operator_symbol>();
                    break;

                case U'(': {
                    std::vector<semantics::concat_chain> nested;
                    ccb.push_last_text();
                    if (auto [error] = process_macro_list(nested); error)
                        return failure;
                    ccb.emplace_back(std::in_place_type<semantics::sublist_conc>, std::move(nested));
                    break;
                }

                case U'\'':
                    if (auto [error] = lex_macro_operand_string(ccb); error)
                        return failure;

                    next_char_special = false;
                    break;

                case U'&': {
                    if (auto [error] = lex_macro_operand_amp(ccb); error)
                        return failure;
                    if (op_name && follows<U'='>())
                        ccb.single_char_push<semantics::equals_conc, hl_scopes::operator_symbol>();
                    else
                        next_char_special = false;
                    break;
                }

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
                    if (!last_char_special)
                    {
                        consume_into(ccb.last_text_value());
                        next_char_special = false;
                        break;
                    }
                    if (auto [error, ncs] = lex_macro_operand_attr(ccb); error)
                        return failure;
                    else
                        next_char_special = ncs;
                    break;

                default:
                    next_char_special = !is_ord();
                    consume_into(ccb.last_text_value());
                    break;
            }
            op_name = false;
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

    result_t_void process_macro_list(std::vector<semantics::concat_chain>& cc)
    {
        assert(follows<U'('>());

        consume(hl_scopes::operator_symbol);
        if (try_consume<U')'>(hl_scopes::operator_symbol))
            return {};

        if (auto [error] = lex_macro_operand(cc.emplace_back(), true, false); error)
            return failure;

        while (try_consume<U','>(hl_scopes::operator_symbol))
        {
            process_optional_line_remark();
            if (auto [error] = lex_macro_operand(cc.emplace_back(), true, false); error)
                return failure;
        }

        if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
            return failure;

        return {};
    }

    result_t_void handle_initial_space(bool reparse)
    {
        if (!reparse && *input.next != U' ')
        {
            add_diagnostic(diagnostic_op::error_S0002);
            consume_rest();
            return failure;
        }

        // skip spaces
        while (follows<U' '>())
            consume();
        adjust_lines();

        return {};
    }

    std::pair<semantics::operand_list, range> macro_ops(bool reparse);

    std::pair<semantics::operand_list, range> ca_expr_ops();
    std::pair<semantics::operand_list, range> ca_branch_ops();
    std::pair<semantics::operand_list, range> ca_var_def_ops();

    constexpr bool is_ord_like(std::span<const semantics::concatenation_point> cc);

    op_data lab_instr();
    void lab_instr_process();
    op_data lab_instr_empty(position start);

    op_data look_lab_instr();
    op_data look_lab_instr_seq();

    result_t_void lex_label_string(concat_chain_builder& cb);
    result_t<semantics::concat_chain> lex_label();
    result_t<semantics::concat_chain> lex_instr();

    void lex_handle_label(semantics::concat_chain cc, range r);
    void lex_handle_instruction(semantics::concat_chain cc, range r);

    op_data lab_instr_rest();

    std::optional<int> maybe_loctr_len()
    {
        if (!holder->proc_status.has_value())
            return std::nullopt;
        auto [_, opcode] = *holder->proc_status;
        return processing::processing_status_cache_key::generate_loctr_len(opcode.value.to_string_view());
    }

    void op_rem_body_deferred();
    void op_rem_body_noop();

    template<result_t<semantics::operand_ptr> (parser2::*first)(),
        result_t<semantics::operand_ptr> (parser2::*rest)() = first>
    std::optional<semantics::op_rem> with_model(bool reparse, bool model_allowed) requires(first != nullptr);

    result_t<semantics::operand_ptr> mach_op();
    result_t<semantics::operand_ptr> dat_op();

    result_t<semantics::operand_ptr> alias_op();
    result_t<semantics::operand_ptr> end_op();
    result_t<semantics::operand_ptr> using_op1();
    result_t<semantics::operand_ptr> asm_mach_expr();
    result_t<semantics::operand_ptr> asm_op();
    result_t<std::unique_ptr<semantics::complex_assembler_operand::component_value_t>> asm_op_inner();
    result_t<std::vector<std::unique_ptr<semantics::complex_assembler_operand::component_value_t>>> asm_op_comma_c();

    [[nodiscard]] constexpr bool ord_followed_by_parenthesis() const noexcept;

    result_t_void lex_rest_of_model_string(concat_chain_builder& ccb);
    result_t<std::optional<semantics::op_rem>> try_model_ops(position line_start);

    void lookahead_operands_and_remarks_dat();
    void lookahead_operands_and_remarks_asm();

    parser2(parser_holder* h)
        : holder(h)
        , input(holder->input_state)
        , data(holder->input.data())
    {}
};

std::pair<semantics::operand_list, range> parser_holder::parser2::macro_ops(bool reparse)
{
    const auto input_start = cur_pos_adjusted();
    if (eof())
        return { semantics::operand_list(), remap_range(range(input_start)) };

    if (auto [error] = handle_initial_space(reparse); error)
        return { semantics::operand_list(), remap_range({ input_start, cur_pos() }) };

    if (eof())
        return { semantics::operand_list(), remap_range(range(cur_pos())) };

    semantics::operand_list result;

    auto line_start = cur_pos(); // already adjusted
    auto start = line_start;
    semantics::concat_chain cc;
    bool pending = true;

    const auto push_operand = [&]() {
        if (!pending)
            return;
        if (cc.empty())
            result.push_back(std::make_unique<semantics::empty_operand>(remap_range({ start, cur_pos() })));
        else
            result.push_back(
                std::make_unique<semantics::macro_operand>(std::move(cc), remap_range({ start, cur_pos() })));
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
                consume(hl_scopes::operator_symbol);
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
                    if (auto [error] = lex_macro_operand(cc, true, false); error)
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
                concat_chain_builder ccb(*this, cc);
                auto& l = ccb.last_text_value();
                do
                {
                    l.push_back(static_cast<char>(*input.next));
                    consume();
                } while (is_ord());
                ccb.push_last_text();
                if (follows<U'='>())
                {
                    ccb.single_char_push<semantics::equals_conc>(); // TODO: no highlighting???
                    next_char_special = true;
                }
                if (const auto n = *input.next; n == EOF_SYMBOL || n == U' ' || n == U',')
                    continue;
                if (auto [error] = lex_macro_operand(cc, next_char_special, false); error)
                {
                    consume_rest();
                    goto end;
                }
                break;
            }

            case U'&':
                if (auto [error] = lex_macro_operand(cc, true, true); error)
                {
                    consume_rest();
                    goto end;
                }
                break;

            case U'\'':
            default:
                if (auto [error] = lex_macro_operand(cc, true, false); error)
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
                std::vector<semantics::concat_chain> nested;
                if (auto [error] = process_macro_list(nested); error)
                {
                    consume_rest();
                    goto end;
                }
                cc.emplace_back(std::in_place_type<semantics::sublist_conc>, std::move(nested));
                break;
            }
        }
    }
end:;
    push_operand();

    return { std::move(result), remap_range({ line_start, cur_pos() }) };
}

parser_holder::parser2::result_t<std::variant<context::id_index, semantics::concat_chain>>
parser_holder::parser2::lex_variable_name(position start)
{
    if (follows<U'('>())
    {
        add_hl_symbol({ start, cur_pos() }, hl_scopes::var_symbol);
        consume(hl_scopes::operator_symbol);
        auto [error, cc] = lex_compound_variable();
        if (error)
            return failure;
        if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
            return failure;
        return std::move(cc);
    }
    else if (!is_ord_first())
    {
        add_diagnostic(diagnostic_op::error_S0008);
        return failure;
    }
    else
    {
        auto [error, id] = lex_id();
        if (error)
            return failure;
        add_hl_symbol({ start, cur_pos() }, hl_scopes::var_symbol);
        return id;
    }
}

parser_holder::parser2::result_t<semantics::vs_ptr> parser_holder::parser2::lex_variable()
{
    assert(follows<U'&'>());

    const auto start = cur_pos_adjusted();
    consume();

    auto [error, var_name] = lex_variable_name(start);
    if (error)
        return failure;

    std::vector<expressions::ca_expr_ptr> sub;
    if (follows<U'('>())
    {
        if (auto [error_sub, sub_] = lex_subscript(); error_sub)
            return failure;
        else
            sub = std::move(sub_);
    }

    const auto r = remap_range({ start, cur_pos() });

    if (std::holds_alternative<context::id_index>(var_name))
        return std::make_unique<semantics::basic_variable_symbol>(
            std::get<context::id_index>(var_name), std::move(sub), r);
    else
    {
        auto& cc = std::get<semantics::concat_chain>(var_name);
        return std::make_unique<semantics::created_variable_symbol>(std::move(cc), std::move(sub), r);
    }
}

std::pair<semantics::operand_list, range> parser_holder::parser2::ca_expr_ops()
{
    const auto input_start = cur_pos_adjusted();
    if (eof())
        return { semantics::operand_list(), remap_range(range(input_start)) };

    if (!lex_optional_space())
    {
        syntax_error_or_eof();
        consume_rest();
        return { semantics::operand_list(), remap_range({ input_start, cur_pos() }) };
    }

    if (eof())
        return { semantics::operand_list(), remap_range(range(cur_pos())) };

    const auto line_start = cur_pos_adjusted();

    semantics::operand_list result;

    bool pending = true;
    while (except<U' '>())
    {
        const auto start = cur_pos();
        if (try_consume<U','>(hl_scopes::operator_symbol))
        {
            if (pending)
                result.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(start))));
            process_optional_line_remark();
            pending = true;
        }
        else
        {
            auto [error, expr] = lex_expr_general();
            if (error)
            {
                const auto r = remap_range({ start, cur_pos() });
                // original fallback
                result.push_back(
                    std::make_unique<semantics::expr_ca_operand>(std::make_unique<expressions::ca_constant>(0, r), r));
                break;
            }
            resolve_expression(expr);
            result.push_back(
                std::make_unique<semantics::expr_ca_operand>(std::move(expr), remap_range({ start, cur_pos() })));
            pending = false;
        }
    }
    if (pending)
        result.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(cur_pos()))));

    consume_rest();

    return { std::move(result), remap_range({ line_start, cur_pos() }) };
}

std::pair<semantics::operand_list, range> parser_holder::parser2::ca_branch_ops()
{
    const auto input_start = cur_pos_adjusted();
    if (eof())
        return { semantics::operand_list(), remap_range(range(input_start)) };

    if (!lex_optional_space())
    {
        syntax_error_or_eof();
        consume_rest();
        return { semantics::operand_list(), remap_range({ input_start, cur_pos() }) };
    }

    if (eof())
        return { semantics::operand_list(), remap_range(range(cur_pos())) };

    const auto line_start = cur_pos_adjusted();

    semantics::operand_list result;

    bool pending = true;
    while (except<U' '>())
    {
        const auto start = cur_pos();
        if (try_consume<U','>(hl_scopes::operator_symbol))
        {
            if (pending)
                result.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(start))));
            process_optional_line_remark();
            pending = true;
            continue;
        }
        else if (!pending)
        {
            syntax_error_or_eof();
            break;
        }
        expressions::ca_expr_ptr first_expr;
        if (follows<U'('>())
        {
            auto [error, e] = lex_expr_list();
            if (error)
                break;
            first_expr = std::move(e);
            resolve_expression(first_expr);
        }
        auto [error, ss] = lex_seq_symbol();
        if (error)
            break;
        const auto r = remap_range({ start, cur_pos() });
        if (first_expr)
            result.push_back(std::make_unique<semantics::branch_ca_operand>(std::move(ss), std::move(first_expr), r));
        else
            result.push_back(std::make_unique<semantics::seq_ca_operand>(std::move(ss), r));
        pending = false;
    }
    if (pending)
        result.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(cur_pos()))));

    consume_rest();

    return { std::move(result), remap_range({ line_start, cur_pos() }) };
}

std::pair<semantics::operand_list, range> parser_holder::parser2::ca_var_def_ops()
{
    const auto input_start = cur_pos_adjusted();
    if (eof())
        return { semantics::operand_list(), remap_range(range(input_start)) };

    if (!lex_optional_space())
    {
        syntax_error_or_eof();
        consume_rest();
        return { semantics::operand_list(), remap_range({ input_start, cur_pos() }) };
    }

    if (eof())
        return { semantics::operand_list(), remap_range(range(cur_pos())) };

    const auto line_start = cur_pos_adjusted();

    semantics::operand_list result;

    bool pending = true;
    while (except<U' '>())
    {
        const auto start = cur_pos();
        if (try_consume<U','>(hl_scopes::operator_symbol))
        {
            if (pending)
                result.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(start))));
            process_optional_line_remark();
            pending = true;
            continue;
        }
        else if (!pending)
        {
            syntax_error_or_eof();
            break;
        }
        (void)try_consume<U'&'>();
        auto [error, var_name] = lex_variable_name(start);
        if (error)
            break;
        std::vector<expressions::ca_expr_ptr> num;
        if (try_consume<U'('>(hl_scopes::operator_symbol))
        {
            if (!is_num())
            {
                syntax_error_or_eof();
                break;
            }
            auto [error_num, num_] = lex_num();
            if (error_num)
                break;
            num.push_back(std::move(num_));
            if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
                break;
        }
        const auto r = remap_range({ start, cur_pos() });
        add_hl_symbol_remapped(r, hl_scopes::var_symbol);
        semantics::vs_ptr var;
        if (std::holds_alternative<context::id_index>(var_name))
        {
            var = std::make_unique<semantics::basic_variable_symbol>(
                std::get<context::id_index>(var_name), std::move(num), r);
        }
        else
        {
            auto& cc = std::get<semantics::concat_chain>(var_name);
            var = std::make_unique<semantics::created_variable_symbol>(std::move(cc), std::move(num), r);
        }
        result.push_back(std::make_unique<semantics::var_ca_operand>(std::move(var), r));
        pending = false;
    }
    if (pending)
        result.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(cur_pos()))));

    consume_rest();

    return { std::move(result), remap_range({ line_start, cur_pos() }) };
}

semantics::operand_list parser_holder::macro_ops(bool reparse)
{
    parser_holder::parser2 p(this);
    auto [ops, line_range] = p.macro_ops(reparse);

    if (!reparse)
    {
        collector.set_operand_remark_field(std::move(ops), std::move(p.remarks), line_range);
        return {};
    }
    else
        return std::move(ops);
}

void parser_holder::op_rem_body_ca_expr()
{
    parser_holder::parser2 p(this);

    auto [ops, line_range] = p.ca_expr_ops();
    collector.set_operand_remark_field(std::move(ops), std::move(p.remarks), line_range);
}

void parser_holder::op_rem_body_ca_branch()
{
    parser_holder::parser2 p(this);

    auto [ops, line_range] = p.ca_branch_ops();
    collector.set_operand_remark_field(std::move(ops), std::move(p.remarks), line_range);
}

void parser_holder::op_rem_body_ca_var_def()
{
    parser_holder::parser2 p(this);

    auto [ops, line_range] = p.ca_var_def_ops();
    collector.set_operand_remark_field(std::move(ops), std::move(p.remarks), line_range);
}

semantics::operand_ptr parser_holder::ca_op_expr()
{
    parser_holder::parser2 p(this);
    const auto start = p.cur_pos_adjusted();
    auto [error, expr] = p.lex_expr_general();
    if (error || *p.input.next != EOF_SYMBOL)
        return nullptr;

    p.resolve_expression(expr);
    return std::make_unique<semantics::expr_ca_operand>(std::move(expr), p.remap_range({ start, p.cur_pos() }));
}

void parser_holder::parser2::lab_instr_process()
{
    assert(follows_PROCESS());

    const auto start = cur_pos_adjusted();
    for (size_t i = 0; i < PROCESS.size(); ++i)
        consume();

    const auto r = remap_range({ start, cur_pos() });
    holder->collector.set_label_field(r);
    holder->collector.set_instruction_field(context::id_index("*PROCESS"), r);
    add_hl_symbol_remapped(r, hl_scopes::instruction);
}

parser_holder::op_data parser_holder::parser2::lab_instr_rest()
{
    if (eof())
    {
        const auto r = remap_range(range(cur_pos()));
        return op_data {
            .op_text = lexing::u8string_with_newlines(),
            .op_range = r,
            .op_logical_column = input.char_position_in_line,
        };
    }

    const auto op_start = cur_pos();
    op_data result {
        .op_text = lexing::u8string_with_newlines(),
        .op_range = {},
        .op_logical_column = input.char_position_in_line,
    };

    while (!eof())
    {
        while (!before_nl())
        {
            result.op_text->text.push_back(lexing::u8string_view_with_newlines::EOLc);
            ++input.line;
            ++input.nl;
            input.char_position_in_line = holder->cont;
            input.char_position_in_line_utf16 = holder->cont;
        }

        const auto ch = *input.next;
        utils::append_utf32_to_utf8(result.op_text->text, ch);

        ++input.next;
        ++input.char_position_in_line;
        input.char_position_in_line_utf16 += 1 + (ch > 0xffffu);
    }

    for (; *input.nl != (size_t)-1;)
    {
        result.op_text->text.push_back(lexing::u8string_view_with_newlines::EOLc);
        ++input.line;
        ++input.nl;
        input.char_position_in_line = holder->cont;
        input.char_position_in_line_utf16 = holder->cont;
    }

    result.op_range = remap_range({ op_start, cur_pos() });

    return result;
}

parser_holder::op_data parser_holder::parser2::lab_instr_empty(position start)
{
    const auto r = remap_range(range(start));

    holder->collector.set_label_field(r);
    holder->collector.set_instruction_field(r);
    holder->collector.set_operand_remark_field(r);

    return {};
}

parser_holder::parser2::result_t_void parser_holder::parser2::lex_label_string(concat_chain_builder& cb)
{
    assert(follows<U'\''>());

    consume_into(cb.last_text_value());

    while (!eof())
    {
        switch (*input.next)
        {
            case U'\'':
                consume_into(cb.last_text_value());
                return {};

            case U'&':
                if (input.next[1] == U'&')
                {
                    consume_into(cb.last_text_value());
                    consume_into(cb.last_text_value());
                }
                else
                {
                    cb.push_last_text();
                    auto [error, vs] = lex_variable();
                    if (error)
                        return failure;
                    cb.emplace_back(std::in_place_type<semantics::var_sym_conc>, std::move(vs));
                }
                break;

            default:
                consume_into(cb.last_text_value());
                break;
        }
    }
    add_diagnostic(diagnostic_op::error_S0005);
    return failure;
}

parser_holder::parser2::result_t<semantics::concat_chain> parser_holder::parser2::lex_label()
{
    semantics::concat_chain chain;
    concat_chain_builder cb(*this, chain, false);

    bool next_char_special = true;

    while (true)
    {
        const auto last_char_special = std::exchange(next_char_special, true);
        switch (*input.next)
        {
            case EOF_SYMBOL:
            case U' ':
                cb.push_last_text();
                return chain;

            case U'.':
                cb.single_char_push<semantics::dot_conc>();
                next_char_special = follows<U'C', U'c'>();
                break;

            case U'=':
                cb.single_char_push<semantics::equals_conc>();
                next_char_special = follows<U'C', U'c'>();
                break;

            case U'&':
                if (input.next[1] == U'&')
                {
                    consume_into(cb.last_text_value());
                    consume_into(cb.last_text_value());
                }
                else
                {
                    cb.push_last_text();
                    auto [error, vs] = lex_variable();
                    if (error)
                        return failure;
                    cb.emplace_back(std::in_place_type<semantics::var_sym_conc>, std::move(vs));
                }
                break;

            case U'\'':
                if (!last_char_special && input.next[1] == U' ')
                    consume_into(cb.last_text_value());
                else if (auto [error] = lex_label_string(cb); error)
                    return failure;
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
                if (last_char_special && input.next[1] == U'\'')
                {
                    consume_into(cb.last_text_value());
                    consume_into(cb.last_text_value());
                    break;
                }
                [[fallthrough]];

            case U'C':
            case U'c':
                if (last_char_special && input.next[1] == U'\'')
                {
                    consume_into(cb.last_text_value());
                    if (auto [error] = lex_label_string(cb); error)
                        return failure;
                    break;
                }
                [[fallthrough]];

            default:
                next_char_special = !is_ord();

                consume_into(cb.last_text_value());

                break;
        }
    }
}

parser_holder::parser2::result_t<semantics::concat_chain> parser_holder::parser2::lex_instr()
{
    if (eof() || follows<U' '>())
    {
        syntax_error_or_eof();
        return failure;
    }

    semantics::concat_chain result;
    concat_chain_builder cb(*this, result, false);

    while (true)
    {
        switch (*input.next)
        {
            case EOF_SYMBOL:
            case U' ':
                cb.push_last_text();
                return result;

            case U'\'':
                syntax_error_or_eof();
                return failure;

            case U'=':
                cb.single_char_push<semantics::equals_conc>();
                break;

            case U'.':
                cb.single_char_push<semantics::dot_conc>();
                break;

            case U'&':
                if (input.next[1] == U'&')
                {
                    consume_into(cb.last_text_value());
                    consume_into(cb.last_text_value());
                }
                else
                {
                    cb.push_last_text();
                    auto [error, vs] = lex_variable();
                    if (error)
                        return failure;
                    cb.emplace_back(std::in_place_type<semantics::var_sym_conc>, std::move(vs));
                }
                break;

            default:
                consume_into(cb.last_text_value());
                break;
        }
    }
}

constexpr bool parser_holder::parser2::is_ord_like(std::span<const semantics::concatenation_point> cc)
{
    if (std::ranges::any_of(
            cc, [](const auto& c) { return !std::holds_alternative<semantics::char_str_conc>(c.value); }))
        return false;
    auto it = std::ranges::find_if(
        cc, [](const auto& c) { return !std::get<semantics::char_str_conc>(c.value).value.empty(); });
    if (it == cc.end())
        return false;
    if (!char_is_ord_first(std::get<semantics::char_str_conc>(it->value).value.front()))
        return false;
    return std::all_of(it, cc.end(), [](const auto& c) {
        const auto& str = std::get<semantics::char_str_conc>(c.value).value;
        return std::ranges::all_of(str, [](unsigned char uc) { return char_is_ord(uc); });
    });
}

parser_holder::op_data parser_holder::parser2::lab_instr()
{
    if (eof())
        return lab_instr_empty(cur_pos());

    if (holder->process_allowed && follows_PROCESS())
    {
        lab_instr_process();
        return lab_instr_rest();
    }

    const auto start = cur_pos();
    auto label_end = start;

    semantics::concat_chain label_concat;
    if (lex_optional_space())
    {
        if (eof())
            return lab_instr_empty(start);
    }
    else
    {
        auto [l_error, v] = lex_label();
        if (l_error)
            return {};

        label_end = cur_pos();
        label_concat = std::move(v);

        if (!lex_optional_space())
        {
            syntax_error_or_eof();
            return {};
        }
    }

    const auto instr_start = cur_pos_adjusted();
    auto [i_error, instr_concat] = lex_instr();
    if (i_error)
        return {};

    lex_handle_label(std::move(label_concat), remap_range({ start, label_end }));
    lex_handle_instruction(std::move(instr_concat), remap_range({ instr_start, cur_pos() }));

    return lab_instr_rest();
}

void parser_holder::parser2::lex_handle_label(semantics::concat_chain cc, range r)
{
    if (cc.empty())
        holder->collector.set_label_field(r);
    else if (std::ranges::any_of(
                 cc, [](const auto& c) { return std::holds_alternative<semantics::var_sym_conc>(c.value); }))
    {
        semantics::concatenation_point::clear_concat_chain(cc);
        for (const auto& c : cc)
        {
            if (!std::holds_alternative<semantics::char_str_conc>(c.value))
                continue;
            add_hl_symbol(std::get<semantics::char_str_conc>(c.value).conc_range, hl_scopes::label);
        }
        holder->collector.set_label_field(std::move(cc), r);
    }
    else if (std::holds_alternative<semantics::dot_conc>(cc.front().value)
        && is_ord_like(std::span(cc).subspan(1))) // seq symbol
    {
        auto& label = std::get<semantics::char_str_conc>(cc[1].value).value;
        for (auto& c : std::span(cc).subspan(2))
            label.append(std::get<semantics::char_str_conc>(c.value).value);

        add_hl_symbol(r, hl_scopes::seq_symbol);
        holder->collector.set_label_field({ parse_identifier(std::move(label), r), r }, r);
    }
    else if (is_ord_like(cc))
    {
        std::string label = semantics::concatenation_point::to_string(std::move(cc));
        add_hl_symbol(r, hl_scopes::label);
        auto id = add_id(label);
        holder->collector.set_label_field({ id, std::move(label) }, r);
    }
    else
    {
        add_hl_symbol(r, hl_scopes::label);
        holder->collector.set_label_field(semantics::concatenation_point::to_string(std::move(cc)), r);
    }
}

void parser_holder::parser2::lex_handle_instruction(semantics::concat_chain cc, range r)
{
    assert(!cc.empty());

    if (std::ranges::any_of(cc, [](const auto& c) { return std::holds_alternative<semantics::var_sym_conc>(c.value); }))
    {
        for (const auto& point : cc)
        {
            if (!std::holds_alternative<semantics::char_str_conc>(point.value))
                continue;
            add_hl_symbol(std::get<semantics::char_str_conc>(point.value).conc_range, hl_scopes::instruction);
        }

        holder->collector.set_instruction_field(std::move(cc), r);
    }
    else if (is_ord_like(std::span(cc).first(1)))
    {
        add_hl_symbol(r, hl_scopes::instruction);
        auto instr_id = parse_identifier(semantics::concatenation_point::to_string(std::move(cc)), r);
        holder->collector.set_instruction_field(instr_id, r);
    }
    else
    {
        add_hl_symbol(r, hl_scopes::instruction);
        auto instr_id = add_id(semantics::concatenation_point::to_string(std::move(cc)));
        holder->collector.set_instruction_field(instr_id, r);
    }
}

parser_holder::op_data parser_holder::parser2::look_lab_instr_seq()
{
    const auto start = cur_pos_adjusted();
    consume();
    if (!is_ord_first())
        return lab_instr_empty(start);

    std::string label = lex_ord();

    const auto seq_end = cur_pos();

    const auto label_r = remap_range({ start, seq_end });
    auto seq_symbol = semantics::seq_sym { parse_identifier(std::move(label), label_r), label_r };
    holder->collector.set_label_field(seq_symbol, label_r);

    if (!lex_optional_space() || !is_ord_first())
    {
        const auto r = remap_range(range(seq_end));
        holder->collector.set_instruction_field(r);
        holder->collector.set_operand_remark_field(r);
        return {};
    }
    const auto instr_start = cur_pos_adjusted();
    std::string instr = lex_ord();
    const auto instr_end = cur_pos();

    if (!eof() && !follows<U' '>())
    {
        const auto r = remap_range(range(seq_end));
        holder->collector.set_instruction_field(r);
        holder->collector.set_operand_remark_field(r);
        return {};
    }

    const auto instr_r = remap_range({ instr_start, instr_end });

    holder->collector.set_instruction_field(parse_identifier(std::move(instr), instr_r), instr_r);

    auto result = lab_instr_rest();

    holder->collector.set_operand_remark_field(result.op_range);

    return result;
}

parser_holder::op_data parser_holder::parser2::look_lab_instr()
{
    const auto start = cur_pos_adjusted();

    std::string label;
    range label_r = remap_range(range(start));
    switch (*input.next)
    {
        case EOF_SYMBOL:
            return lab_instr_empty(start);
        case U'.':
            return look_lab_instr_seq();

        default:
            if (!is_ord_first())
                return lab_instr_empty(start);
            label = lex_ord();
            label_r = remap_range({ start, cur_pos() });
            break;

        case U' ':
            break;
    }

    if (!lex_optional_space())
        return lab_instr_empty(start);
    if (!is_ord_first())
        return lab_instr_empty(start);

    const auto instr_start = cur_pos_adjusted();
    auto instr = lex_ord();
    const auto instr_r = remap_range({ instr_start, cur_pos() });

    if (!eof() && !follows<U' '>())
        return lab_instr_empty(start);

    if (!label.empty())
    {
        const auto id = add_id(label);
        holder->collector.set_label_field(semantics::ord_symbol_string { id, std::move(label) }, label_r);
    }
    holder->collector.set_instruction_field(parse_identifier(std::move(instr), instr_r), instr_r);

    auto result = lab_instr_rest();

    holder->collector.set_operand_remark_field(result.op_range);

    return result;
}

parser_holder::op_data parser_holder::lab_instr()
{
    parser_holder::parser2 p(this);

    // TODO: diagnose instruction not finished on the initial line
    // const auto initial_state = p.input;

    return p.lab_instr();
}

parser_holder::op_data parser_holder::look_lab_instr()
{
    parser_holder::parser2 p(this);

    return p.look_lab_instr();
}

void parser_holder::parser2::op_rem_body_deferred()
{
    const auto start = cur_pos_adjusted();
    if (eof())
    {
        holder->collector.set_operand_remark_field(remap_range(range(start)));
        return;
    }
    if (!follows<U' '>())
    {
        syntax_error_or_eof();
        return;
    }
    while (follows<U' '>())
        consume();

    auto rest = parser2(*this).lab_instr_rest();

    std::vector<semantics::vs_ptr> vs;

    bool next_char_special = true;

    while (!eof())
    {
        const auto last_char_special = std::exchange(next_char_special, true);
        switch (*input.next)
        {
            case U',':
                consume(hl_scopes::operator_symbol);
                process_optional_line_remark();
                break;

            case U' ':
                lex_last_remark();
                break;

            case U'*':
            case U'/':
            case U'+':
            case U'-':
            case U'=':
            case U'.':
            case U'(':
            case U')':
                consume(hl_scopes::operator_symbol);
                break;

            case U'\'': {
                disable_ca_string();
                utils::scope_exit e([this]() noexcept { enable_ca_string(); });

                const auto string_start = cur_pos_adjusted();
                consume();

                while (true)
                {
                    switch (*input.next)
                    {
                        case EOF_SYMBOL:
                            syntax_error_or_eof();
                            add_hl_symbol({ string_start, cur_pos() }, hl_scopes::string);
                            return;

                        case U'\'':
                            if (input.next[1] != U'\'')
                                goto done;
                            consume();
                            consume();
                            break;

                        case U'&':
                            if (input.next[1] == U'&')
                            {
                                consume();
                                consume();
                            }
                            else if (auto [error, v] = lex_variable(); error)
                                return;
                            else
                                vs.push_back(std::move(v));
                            break;

                        default:
                            consume();
                            break;
                    }
                }

            done:
                consume();

                add_hl_symbol({ string_start, cur_pos() }, hl_scopes::string);
                break;
            }

            case U'&': {
                const auto amp = cur_pos_adjusted();
                switch (input.next[1])
                {
                    case EOF_SYMBOL:
                        consume();
                        add_diagnostic(diagnostic_op::error_S0003);
                        return;

                    case U'&':
                        consume();
                        consume();
                        break;

                    default:
                        if (auto [error, v] = lex_variable(); error)
                            return;
                        else
                        {
                            vs.push_back(std::move(v));
                            const auto r = remap_range({ amp, cur_pos() });
                            add_hl_symbol(r, hl_scopes::var_symbol);
                        }
                        break;
                }
            }
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
                if (last_char_special && follows<mach_attrs, group<U'\''>, attr_argument>())
                {
                    const auto p = cur_pos_adjusted();
                    consume();
                    consume();
                    add_hl_symbol({ p, cur_pos() }, hl_scopes::data_attr_type);
                    next_char_special = false;
                    break;
                }
                [[fallthrough]];
            default: {
                const auto substart = cur_pos_adjusted();
                while (except<U'&', U' ', U',', U'*', U'/', U'+', U'-', U'=', U'.', U'(', U')', U'\''>())
                {
                    next_char_special = !is_ord();
                    consume();
                    if (next_char_special)
                        break;
                }
                add_hl_symbol({ substart, cur_pos() }, hl_scopes::operand);
            }
        }
    }
    holder->collector.set_operand_remark_field(
        std::move(*rest.op_text), std::move(vs), std::move(remarks), rest.op_range, rest.op_logical_column);
}

void parser_holder::op_rem_body_deferred()
{
    parser_holder::parser2 p(this);

    p.op_rem_body_deferred();
}

void parser_holder::parser2::op_rem_body_noop()
{
    (void)lex_optional_space();

    if (eof())
    {
        const auto r = remap_range(range(cur_pos()));
        holder->collector.set_operand_remark_field(semantics::operand_list(), semantics::remark_list(), r);
    }
    else
    {
        const auto start = cur_pos_adjusted();
        while (!eof())
            consume();

        const auto r = remap_range({ start, cur_pos() });
        holder->collector.set_operand_remark_field(semantics::operand_list {}, semantics::remark_list { r }, r);
    }
}

void parser_holder::op_rem_body_noop()
{
    parser_holder::parser2 p(this);

    p.op_rem_body_noop();
}

parser_holder::parser2::result_t_void parser_holder::parser2::lex_rest_of_model_string(concat_chain_builder& ccb)
{
    while (true)
    {
        switch (*input.next)
        {
            case EOF_SYMBOL:
                return failure;

            case U'&':
                if (input.next[1] == U'&')
                {
                    consume_into(ccb.last_text_value());
                    consume_into(ccb.last_text_value());
                    break;
                }
                if (auto [error, vs] = lex_variable(); error)
                    return failure;
                else
                    ccb.emplace_back(std::in_place_type<semantics::var_sym_conc>, std::move(vs));
                break;

            case U'.':
                ccb.single_char_push<semantics::dot_conc>();
                break;

            case U'=':
                ccb.single_char_push<semantics::equals_conc>();
                break;

            case U'\'':
                consume_into(ccb.last_text_value());
                return {};

            default:
                consume_into(ccb.last_text_value());
                break;
        }
    }
}

parser_holder::parser2::result_t<std::optional<semantics::op_rem>> parser_holder::parser2::try_model_ops(
    position line_start)
{
    const auto start = cur_pos_adjusted();
    const auto initial = input.next;

    semantics::concat_chain cc;
    concat_chain_builder ccb(*this, cc);
    bool next_char_special = true;
    std::optional<position> in_string;
    while (true)
    {
        const bool last_char_special = std::exchange(next_char_special, true);
        switch (*input.next)
        {
            case EOF_SYMBOL:
            case U' ':
                return std::nullopt;

            case U'&':
                if (input.next[1] == U'&')
                {
                    consume();
                    consume();
                    break;
                }
                else
                    goto done;

            case U'\'': {
                in_string = cur_pos_adjusted();
                consume();
                while (except<U'\''>())
                {
                    if (follows<group<U'&'>, group<U'&'>>())
                        consume();
                    else if (follows<U'&'>())
                        goto done;
                    consume();
                }
                if (!match<U'\''>(diagnostic_op::error_S0005))
                    return failure;
                add_hl_symbol(remap_range({ *in_string, cur_pos() }), hl_scopes::string);
                in_string.reset();
                break;
            }

            case U',':
            case U'(':
            case U')':
                consume(hl_scopes::operator_symbol);
                break;

            case U'=':
                consume();
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
                consume();
                if (last_char_special && follows<group<U'\''>, attr_argument>())
                    consume();
                next_char_special = false;
                break;

            default:
                next_char_special = !is_ord();
                consume();
                break;
        }
    }
done:;
    assert(follows<U'&'>());

    if (initial != input.next)
        cc.emplace_back(
            std::in_place_type<semantics::char_str_conc>, capture_text(initial), remap_range({ start, cur_pos() }));

    if (auto [error, vs] = lex_variable(); error)
        return failure;
    else
        ccb.emplace_back(std::in_place_type<semantics::var_sym_conc>, std::move(vs));

    if (in_string)
    {
        auto [error] = lex_rest_of_model_string(ccb);
        add_hl_symbol(remap_range({ *in_string, cur_pos() }), hl_scopes::string);
        if (error)
            return failure;
        in_string.reset();
    }

    std::optional<position> operand_end;
    while (true)
    {
        const bool last_char_special = std::exchange(next_char_special, true);
        switch (*input.next)
        {
            case U' ':
                operand_end.emplace(cur_pos());
                lex_last_remark();
                [[fallthrough]];

            case EOF_SYMBOL: {
                if (!operand_end)
                    operand_end.emplace(cur_pos());
                semantics::op_rem result {
                    .operands = {},
                    .remarks = std::move(remarks),
                    .line_range = remap_range({ line_start, cur_pos() }),
                };
                semantics::concatenation_point::clear_concat_chain(cc);
                resolve_concat_chain(cc);
                result.operands.emplace_back(std::make_unique<semantics::model_operand>(
                    std::move(cc), holder->line_limits, remap_range({ start, *operand_end })));
                return result;
            }

            case U'&':
                if (input.next[1] == U'&')
                {
                    consume_into(ccb.last_text_value());
                    consume_into(ccb.last_text_value());
                }
                else if (auto [error, vs] = lex_variable(); error)
                    return failure;
                else
                    ccb.emplace_back(std::in_place_type<semantics::var_sym_conc>, std::move(vs));
                break;

            case U'\'': {
                const auto string_start = cur_pos_adjusted();
                consume_into(ccb.last_text_value());
                auto [error] = lex_rest_of_model_string(ccb);
                add_hl_symbol(remap_range({ string_start, cur_pos() }), hl_scopes::string);
                if (error)
                    return failure;
                break;
            }

            case U'(':
            case U')':
            case U',':
                consume_into(ccb.last_text_value(), hl_scopes::operator_symbol);
                break;


            case U'.':
                ccb.single_char_push<semantics::dot_conc>();
                break;

            case U'=':
                ccb.single_char_push<semantics::equals_conc>();
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
                consume_into(ccb.last_text_value());
                if (last_char_special && follows<group<U'\''>, attr_argument>())
                    consume_into(ccb.last_text_value());
                next_char_special = false;
                break;

            default:
                next_char_special = !is_ord();
                consume_into(ccb.last_text_value());
                break;
        }
    }
}

parser_holder::parser2::result_t<semantics::operand_ptr> parser_holder::parser2::mach_op()
{
    const auto start = cur_pos_adjusted();
    auto [disp_error, disp] = lex_mach_expr();
    if (disp_error)
        return failure;

    if (!try_consume<U'('>(hl_scopes::operator_symbol))
        return std::make_unique<semantics::expr_machine_operand>(std::move(disp), remap_range({ start, cur_pos() }));

    expressions::mach_expr_ptr e1, e2;
    if (eof())
    {
        syntax_error_or_eof();
        return failure;
    }

    if (except<U','>())
    {
        if (auto [error, e] = lex_mach_expr(); error)
            return failure;
        else
            e1 = std::move(e);
    }
    bool parsed_comma = false;
    if ((parsed_comma = try_consume<U','>(hl_scopes::operator_symbol)) && !follows<U')'>())
    {
        if (auto [error, e] = lex_mach_expr(); error)
            return failure;
        else
            e2 = std::move(e);
    }

    if (!match<U')'>(hl_scopes::operator_symbol, diagnostic_op::error_S0011))
        return failure;

    if (e1 && e2)
        return std::make_unique<semantics::address_machine_operand>(std::move(disp),
            std::move(e1),
            std::move(e2),
            remap_range({ start, cur_pos() }),
            checking::operand_state::PRESENT);
    if (e2)
        return std::make_unique<semantics::address_machine_operand>(std::move(disp),
            nullptr,
            std::move(e2),
            remap_range({ start, cur_pos() }),
            checking::operand_state::FIRST_OMITTED);

    if (e1 && !parsed_comma)
        return std::make_unique<semantics::address_machine_operand>(std::move(disp),
            nullptr,
            std::move(e1),
            remap_range({ start, cur_pos() }),
            checking::operand_state::ONE_OP);

    return std::make_unique<semantics::address_machine_operand>(std::move(disp),
        std::move(e1),
        nullptr,
        remap_range({ start, cur_pos() }),
        checking::operand_state::SECOND_OMITTED);
}

parser_holder::parser2::result_t<semantics::operand_ptr> parser_holder::parser2::dat_op()
{
    const auto start = cur_pos_adjusted();
    const auto disabled = disable_literals();

    auto [error, d] = lex_data_definition(false);
    if (error)
        return failure;
    return std::make_unique<semantics::data_def_operand_inline>(std::move(d), remap_range({ start, cur_pos() }));
}
template<parser_holder::parser2::result_t<semantics::operand_ptr> (parser_holder::parser2::*first)(),
    parser_holder::parser2::result_t<semantics::operand_ptr> (parser_holder::parser2::*rest)()>
std::optional<semantics::op_rem> parser_holder::parser2::with_model(bool reparse, bool model_allowed)
    requires(first != nullptr)
{
    const auto start = cur_pos();
    if (eof())
        return semantics::op_rem { .line_range = remap_range(range(start)) };

    if (auto [error] = handle_initial_space(reparse); error)
        return std::nullopt;

    if (eof())
        return semantics::op_rem { .line_range = remap_range(range(start)) };

    if (model_allowed && std::find(input.next, input.last, U'&') != input.last)
    {
        auto model_parser = *this;
        if (auto [error, result] = model_parser.try_model_ops(start); error)
            return std::nullopt;
        else if (result)
            return std::move(*result);
        holder->collector.prepare_for_next_statement();
    }

    std::vector<semantics::operand_ptr> operands;

    bool has_error = false;
    if (follows<U','>())
        operands.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(cur_pos()))));
    else if (auto [error, op] = (this->*first)(); (has_error = error))
        operands.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(cur_pos()))));
    else
        operands.push_back(std::move(op));

    if (!has_error && ((rest == nullptr && except<U' '>()) || except<U',', U' '>()))
    {
        has_error = true;
        syntax_error_or_eof();
    }

    while (!has_error && follows<U','>())
    {
        consume(hl_scopes::operator_symbol);
        if (follows<U','>())
            operands.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(cur_pos()))));
        else if (eof() || follows<U' '>())
        {
            operands.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(cur_pos()))));
            break;
        }
        else if (auto [error_inner, op_inner] = (this->*rest)(); error_inner)
        {
            operands.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(cur_pos()))));
            break;
        }
        else
        {
            operands.push_back(std::move(op_inner));
            if (except<U',', U' '>())
            {
                syntax_error_or_eof();
                break;
            }
        }
    }

    consume_rest();

    return semantics::op_rem {
        .operands = std::move(operands),
        .remarks = std::move(remarks),
        .line_range = remap_range({ start, cur_pos() }),
    };
}
std::optional<semantics::op_rem> parser_holder::op_rem_body_mach(bool reparse, bool model_allowed)
{
    parser_holder::parser2 p(this);

    return p.with_model<&parser2::mach_op>(reparse, model_allowed);
}

std::optional<semantics::op_rem> parser_holder::op_rem_body_dat(bool reparse, bool model_allowed)
{
    parser_holder::parser2 p(this);

    return p.with_model<&parser2::dat_op>(reparse, model_allowed);
}

parser_holder::parser2::result_t<semantics::operand_ptr> parser_holder::parser2::alias_op()
{
    const auto start = cur_pos_adjusted();
    const auto initial = input.next;

    if (!match<U'C', U'c', U'X', U'x'>(hl_scopes::self_def_type))
        return failure;

    if (!must_follow<U'\''>())
        return failure;

    auto [error, s] = lex_simple_string();
    if (error)
        return failure;

    const auto r = remap_range({ start, cur_pos() });
    add_hl_symbol_remapped(r, hl_scopes::self_def_type);

    return std::make_unique<semantics::expr_assembler_operand>(
        std::make_unique<expressions::mach_expr_default>(r), capture_text(initial), r);
}

parser_holder::parser2::result_t<semantics::operand_ptr> parser_holder::parser2::end_op()
{
    const auto start = cur_pos_adjusted();
    if (!match<U'('>(hl_scopes::operator_symbol))
        return failure;

    std::string s1, s2, s3;

    auto op_start = cur_pos_adjusted();
    while (except<U','>())
        consume_into(s1);
    const auto r1 = remap_range({ op_start, cur_pos() });

    if (!match<U','>(hl_scopes::operator_symbol))
        return failure;

    op_start = cur_pos_adjusted();
    while (except<U','>())
        consume_into(s2);
    const auto r2 = remap_range({ op_start, cur_pos() });

    if (!match<U','>(hl_scopes::operator_symbol))
        return failure;

    op_start = cur_pos_adjusted();
    while (except<U')'>())
        consume_into(s3);
    const auto r3 = remap_range({ op_start, cur_pos() });

    if (!match<U')'>(hl_scopes::operator_symbol))
        return failure;

    std::vector<std::unique_ptr<semantics::complex_assembler_operand::component_value_t>> language_triplet;
    language_triplet.push_back(
        std::make_unique<semantics::complex_assembler_operand::string_value_t>(std::move(s1), r1));
    language_triplet.push_back(
        std::make_unique<semantics::complex_assembler_operand::string_value_t>(std::move(s2), r2));
    language_triplet.push_back(
        std::make_unique<semantics::complex_assembler_operand::string_value_t>(std::move(s3), r3));
    add_hl_symbol_remapped(r1, hl_scopes::operand);
    add_hl_symbol_remapped(r2, hl_scopes::operand);
    add_hl_symbol_remapped(r3, hl_scopes::operand);

    const auto r = remap_range({ start, cur_pos() });
    return std::make_unique<semantics::complex_assembler_operand>("", std::move(language_triplet), r);
}

parser_holder::parser2::result_t<semantics::operand_ptr> parser_holder::parser2::using_op1()
{
    const auto start = cur_pos_adjusted();
    if (!try_consume<U'('>(hl_scopes::operator_symbol))
        return asm_mach_expr();

    const auto initial1 = input.next;
    auto [error1, e1] = lex_mach_expr();
    if (error1)
        return failure;
    const auto end1 = input.next;

    if (!try_consume<U','>(hl_scopes::operator_symbol))
    {
        if (!match<U')'>(hl_scopes::operator_symbol))
            return failure;

        const auto r = remap_range({ start, cur_pos() });
        return std::make_unique<semantics::expr_assembler_operand>(
            std::move(e1), utils::to_upper_copy(capture_text(initial1, end1)), r);
    }

    const auto initial2 = input.next;
    auto [error2, e2] = lex_mach_expr();
    if (error2)
        return failure;
    const auto end2 = input.next;

    if (!match<U')'>(hl_scopes::operator_symbol))
        return failure;

    const auto r = remap_range({ start, cur_pos() });
    return std::make_unique<semantics::using_instr_assembler_operand>(
        std::move(e1), std::move(e2), capture_text(initial1, end1), capture_text(initial2, end2), r);
}

parser_holder::parser2::result_t<semantics::operand_ptr> parser_holder::parser2::asm_mach_expr()
{
    const auto start = cur_pos_adjusted();
    const auto initial = input.next;

    auto [error, expr] = lex_mach_expr();
    if (error)
        return failure;

    const auto r = remap_range({ start, cur_pos() });
    return std::make_unique<semantics::expr_assembler_operand>(
        std::move(expr), utils::to_upper_copy(capture_text(initial)), r);
}

constexpr bool parser_holder::parser2::ord_followed_by_parenthesis() const noexcept
{
    if (!is_ord_first())
        return false;

    const auto* p = input.next;

    while (char_is_ord(*p))
        ++p;

    return *p == U'(';
}

parser_holder::parser2::result_t<std::unique_ptr<semantics::complex_assembler_operand::component_value_t>>
parser_holder::parser2::asm_op_inner()
{
    const auto start = cur_pos_adjusted();
    if (follows<U'\''>())
    {
        auto [error, s] = lex_simple_string();
        if (error)
            return failure;
        const auto r = remap_range({ start, cur_pos() });
        return std::make_unique<semantics::complex_assembler_operand::string_value_t>(std::move(s), r);
    }
    if (is_num())
    {
        const auto [error, v] = lex_number_as_int();
        if (error)
            return failure;

        const auto& [n, r] = v;
        add_hl_symbol_remapped(r, hl_scopes::operand);
        return std::make_unique<semantics::complex_assembler_operand::int_value_t>(n, r);
    }

    if (follows<U',', U')'>())
        return std::make_unique<semantics::complex_assembler_operand::string_value_t>("", remap_range(range(start)));

    if (!is_ord_first())
    {
        syntax_error_or_eof();
        return failure;
    }

    auto id = lex_ord_upper();

    add_hl_symbol({ start, cur_pos() }, hl_scopes::operand);

    if (!try_consume<U'('>(hl_scopes::operator_symbol))
        return std::make_unique<semantics::complex_assembler_operand::string_value_t>(
            std::move(id), remap_range({ start, cur_pos() }));

    auto [error, nested] = asm_op_comma_c();
    if (error)
        return failure;

    if (!match<U')'>(hl_scopes::operator_symbol))
        return failure;

    const auto r = remap_range({ start, cur_pos() });
    return std::make_unique<semantics::complex_assembler_operand::composite_value_t>(
        std::move(id), std::move(nested), r);
}
parser_holder::parser2::result_t<std::vector<std::unique_ptr<semantics::complex_assembler_operand::component_value_t>>>
parser_holder::parser2::asm_op_comma_c()
{
    std::vector<std::unique_ptr<semantics::complex_assembler_operand::component_value_t>> result;
    if (auto [error, op] = asm_op_inner(); error)
        return failure;
    else
        result.push_back(std::move(op));

    while (try_consume<U','>(hl_scopes::operator_symbol))
    {
        if (auto [error, op] = asm_op_inner(); error)
            return failure;
        else
            result.push_back(std::move(op));
    }

    return result;
}


parser_holder::parser2::result_t<semantics::operand_ptr> parser_holder::parser2::asm_op()
{
    const auto start = cur_pos_adjusted();
    if (follows<U'\''>())
    {
        auto [error, s] = lex_simple_string();
        if (error)
            return failure;
        return std::make_unique<semantics::string_assembler_operand>(std::move(s), remap_range({ start, cur_pos() }));
    }

    if (!ord_followed_by_parenthesis())
        return asm_mach_expr();

    auto id = lex_ord_upper();
    add_hl_symbol({ start, cur_pos() }, hl_scopes::operand);

    if (!match<U'('>(hl_scopes::operator_symbol))
        return failure;

    auto [error, nested] = asm_op_comma_c();
    if (error)
        return failure;

    if (!match<U')'>(hl_scopes::operator_symbol))
        return failure;

    const auto r = remap_range({ start, cur_pos() });
    return std::make_unique<semantics::complex_assembler_operand>(std::move(id), std::move(nested), r);
}

std::optional<semantics::op_rem> parser_holder::op_rem_body_asm(
    context::id_index opcode, bool reparse, bool model_allowed)
{
    parser_holder::parser2 p(this);

    static constexpr context::id_index ALIAS("ALIAS");
    static constexpr context::id_index USING("USING");
    static constexpr context::id_index END("END");

    if (opcode == ALIAS)
        return p.with_model<&parser2::alias_op, nullptr>(reparse, model_allowed);
    else if (opcode == USING)
        return p.with_model<&parser2::using_op1, &parser2::asm_mach_expr>(reparse, model_allowed);
    else if (opcode == END)
        return p.with_model<&parser2::asm_op, &parser2::end_op>(reparse, model_allowed);
    else
        return p.with_model<&parser2::asm_op>(reparse, model_allowed);
}

semantics::operand_ptr parser_holder::operand_mach()
{
    parser_holder::parser2 p(this);
    auto [error, op] = p.mach_op();
    if (error || *p.input.next != EOF_SYMBOL)
        return nullptr;

    return std::move(op);
}

void parser_holder::parser2::lookahead_operands_and_remarks_dat()
{
    const auto start = cur_pos();
    if (eof() || !lex_optional_space() || eof())
    {
        holder->collector.set_operand_remark_field({}, {}, remap_range(range(start)));
        return;
    }
    auto [error, op] = dat_op();
    if (error)
    {
        holder->collector.set_operand_remark_field({}, {}, remap_range(range(start)));
        return;
    }
    semantics::operand_list operands;
    operands.push_back(std::move(op));
    range r = remap_range({ start, cur_pos() });
    holder->collector.set_operand_remark_field(std::move(operands), std::vector<range>(), r);
}

void parser_holder::lookahead_operands_and_remarks_dat()
{
    parser2 p(this);

    p.lookahead_operands_and_remarks_dat();
}

void parser_holder::parser2::lookahead_operands_and_remarks_asm()
{
    auto start = cur_pos_adjusted();
    if (eof() || !lex_optional_space() || eof())
    {
        holder->collector.set_operand_remark_field({}, {}, remap_range(range(start)));
        return;
    }
    semantics::operand_list operands;

    bool errors = false;
    auto op_start = cur_pos_adjusted();

    if (auto [error, op] = asm_op(); error)
    {
        errors = true;
        operands.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(op_start))));
        while (except<U',', U' '>())
            consume();
    }
    else
        operands.push_back(std::move(op));

    while (follows<U','>())
    {
        consume();
        op_start = cur_pos_adjusted();
        if (follows<U',', U' '>())
        {
            operands.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(op_start))));
            continue;
        }
        auto [error, op] = asm_op();
        if (error || errors)
        {
            errors = true;
            operands.push_back(std::make_unique<semantics::empty_operand>(remap_range(range(op_start))));
            while (except<U',', U' '>())
                consume();
        }
        else
            operands.push_back(std::move(op));
    }
    const range r = remap_range({ start, cur_pos() });
    holder->collector.set_operand_remark_field(std::move(operands), std::vector<range>(), r);
}

void parser_holder::lookahead_operands_and_remarks_asm()
{
    parser2 p(this);

    p.lookahead_operands_and_remarks_asm();
}

semantics::literal_si parser_holder::literal_reparse()
{
    parser2 p(this);

    auto [error, lit] = p.lex_literal();
    if (error || !p.eof())
        return nullptr;
    return std::move(lit);
}

expressions::ca_expr_ptr parser_holder::testing_expr()
{
    parser2 p(this);
    auto [error, e] = p.lex_expr();
    if (error)
    {
        p.add_diagnostic(diagnostic_op::error_S0002);
        return {};
    }
    else
        return std::move(e);
}

expressions::data_definition parser_holder::testing_data_def()
{
    parser2 p(this);
    auto [error, dd] = p.lex_data_definition(false);
    if (error)
    {
        p.add_diagnostic(diagnostic_op::error_S0002);
        return {};
    }
    else
        return std::move(dd);
}

} // namespace hlasm_plugin::parser_library::parsing
