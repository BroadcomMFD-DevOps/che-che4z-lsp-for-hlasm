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

#include "lexer.h"

#include <array>
#include <assert.h>
#include <string>
#include <utility>

#include "token.h"
#include "utils/string_operations.h"
#include "utils/unicode_text.h"

using namespace hlasm_plugin;
using namespace parser_library;
using namespace lexing;

lexer::lexer() { reset(false, {}, 0, false); }

namespace {
lexer::char_substitution append_utf8_to_utf32(std::vector<char_t>& t, std::string_view s)
{
    lexer::char_substitution subs {};

    while (!s.empty())
    {
        unsigned char c = s.front();
        if (c < 0x80)
        {
            t.push_back(c);
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
        }
        else
        {
            subs.server = true;
            t.push_back(utils::substitute_character);
            s.remove_prefix(1);
        }
    }

    return subs;
}
} // namespace

void lexer::reset(bool unlimited_lines, position file_offset, size_t logical_column, bool process_allowed)
{
    tokens.clear();
    retired_tokens.clear();
    last_token_id_ = 0;
    line_limits.clear();

    unlimited_line_ = unlimited_lines;
    process_allowed_ = process_allowed;

    input_.push_back(EOF_SYMBOL);
    input_state_.next = input_.data();
    input_state_.line = (size_t)file_offset.line;
    input_state_.char_position_in_line = logical_column;
    input_state_.char_position_in_line_utf16 = (size_t)file_offset.column;

    token_start_state_ = input_state_;

    last_line = input_state_;
    last_line.line = -1;
}

lexer::char_substitution lexer::reset(
    std::string_view str, bool unlimited_lines, position file_offset, size_t logical_column, bool process_allowed)
{
    input_.clear();
    auto result = append_utf8_to_utf32(input_, str);

    reset(unlimited_lines, file_offset, logical_column, process_allowed);

    return result;
}

lexer::char_substitution lexer::reset(
    const logical_line<utils::utf8_iterator<std::string_view::iterator, utils::utf8_utf16_counter>>& l,
    bool unlimited_lines,
    position file_offset,
    size_t logical_column,
    bool process_allowed)
{
    char_substitution subs {};

    input_.clear();

    for (size_t i = 0; i < l.segments.size(); ++i)
    {
        const auto& s = l.segments[i];
        if (i > 0)
            input_.insert(input_.end(), std::ranges::distance(s.begin, s.code), s.continuation_error ? 'X' : ' ');

        subs |= append_utf8_to_utf32(input_, std::string_view(s.code.base(), s.end.base()));

        if (i + 1 < l.segments.size())
        {
            // do not add the last EOL
            using enum hlasm_plugin::parser_library::lexing::logical_line_segment_eol;
            switch (s.eol)
            {
                case none:
                    break;
                case lf:
                    input_.push_back('\n');
                    break;
                case crlf:
                    input_.push_back('\r');
                    input_.push_back('\n');
                    break;
                case cr:
                    input_.push_back('\r');
                    break;
            }
        }
    }

    reset(unlimited_lines, file_offset, logical_column, process_allowed);

    return subs;
}

void lexer::create_token(int ttype, unsigned channel)
{
    /* do not generate empty tokens (except EOF) */
    if (input_state_.next == token_start_state_.next && (size_t)ttype != antlr4::Token::EOF)
        return;

    creating_var_symbol_ = ttype == AMPERSAND;
    if (creating_attr_ref_)
        creating_attr_ref_ = ttype == IGNORED || ttype == CONTINUATION;

    const auto& end = token_start_state_.line == input_state_.line ? input_state_ : last_line;

    if (tokens.size() == tokens.capacity())
    {
        if (tokens.empty())
            tokens.reserve(4096 / sizeof(token));
        else
        {
            // The parser definitely stores addresses of tokens, so we need to preserve them until reset.
            // However, we no longer rely on address alone to identify identical tokens,
            // Therefore, we just need to copy the values from the old vector to the new one to preserve
            // value accessible via index.
            const auto& old_tokens = retired_tokens.emplace_back(std::move(tokens));
            tokens.reserve(old_tokens.capacity() * 2);
            for (const auto& t : old_tokens)
                tokens.emplace_back(t);
        }
    }

    tokens.emplace_back(this,
        ttype,
        channel,
        token_start_state_.next - input_.data(),
        input_state_.next - input_.data(),
        token_start_state_.line,
        token_start_state_.char_position_in_line,
        last_token_id_,
        token_start_state_.char_position_in_line_utf16,
        end.char_position_in_line_utf16);

    ++last_token_id_;
}

void lexer::consume()
{
    const auto next = *input_state_.next;
    if (next == EOF_SYMBOL)
        return;

    if (next == '\n')
    {
        last_line = input_state_;
        ++last_line.char_position_in_line;
        ++last_line.char_position_in_line_utf16;
        input_state_.line++;
        input_state_.char_position_in_line = 0;
        input_state_.char_position_in_line_utf16 = 0;
    }
    else
    {
        input_state_.char_position_in_line++;
        input_state_.char_position_in_line_utf16 += 1 + (next > 0xFFFF);
    }

    ++input_state_.next;
}

bool lexer::eof() const { return *input_state_.next == EOF_SYMBOL; }

/* set start token info */
void lexer::start_token() { token_start_state_ = input_state_; }

/*
main logic
returns already lexed token from token queue
or if empty, tries to lex next token from input
*/
bool lexer::more_tokens()
{
    // capture lexer state before the start of token lexing
    // so that we know where the currently lexed token begins
    start_token();

    if (eof())
    {
        create_token((int)antlr4::Token::EOF);
        return false;
    }

    else if (double_byte_enabled_)
        check_continuation();

    else if (!unlimited_line_ && input_state_.char_position_in_line == end_ && *input_state_.next != ' '
        && continuation_enabled_)
        lex_continuation();

    else if ((unlimited_line_ && (*input_state_.next == '\r' || *input_state_.next == '\n'))
        || (!unlimited_line_ && input_state_.char_position_in_line >= end_))
        lex_end();

    else if (input_state_.char_position_in_line < begin_)
        lex_begin();

    else
        // lex non-special tokens
        lex_tokens();

    return true;
}

void lexer::lex_tokens()
{
    switch (*input_state_.next)
    {
        case '*':
            if (input_state_.char_position_in_line == begin_ && is_process())
            {
                lex_process();
            }
            else
            {
                consume();
                create_token(ASTERISK);
            }
            break;

        case '.':
            consume();
            create_token(DOT);
            break;

        case ' ':
            lex_space();
            break;

        case '-':
            consume();
            create_token(MINUS);
            break;

        case '+':
            consume();
            create_token(PLUS);
            break;

        case '=':
            consume();
            create_token(EQUALS);
            break;

        case '<':
            consume();
            create_token(LT);
            break;

        case '>':
            consume();
            create_token(GT);
            break;

        case ',':
            consume();
            create_token(COMMA);
            break;

        case '(':
            consume();
            create_token(LPAR);
            break;

        case ')':
            consume();
            create_token(RPAR);
            break;

        case '\'':
            consume();
            if (!creating_attr_ref_)
            {
                create_token(APOSTROPHE);
            }
            else
                create_token(ATTR);
            break;

        case '/':
            consume();
            create_token(SLASH);
            break;

        case '&':
            consume();
            create_token(AMPERSAND);
            break;

        case '\r':
            consume();
            if (*input_state_.next == '\n')
                consume();
            break;
        case '\n':
            consume();
            break;

        case '|':
            consume();
            create_token(VERTICAL);
            break;

        default:
            lex_word();
            break;
    }
}

void lexer::lex_begin()
{
    start_token();
    while (input_state_.char_position_in_line < begin_ && !eof() && *input_state_.next != '\n')
        consume();
    create_token(IGNORED, HIDDEN_CHANNEL);
}

void lexer::lex_end()
{
    start_token();
    while (*input_state_.next != '\n' && !eof())
        consume();

    if (!eof())
    {
        consume();
    }
    if (double_byte_enabled_)
        check_continuation();
    create_token(IGNORED, HIDDEN_CHANNEL);
}

/* lex continuation and ignores */
void lexer::lex_continuation()
{
    start_token();
    line_limits.push_back(token_start_state_.char_position_in_line_utf16);

    /* lex continuation */
    while (input_state_.char_position_in_line <= end_default_ && !eof())
        consume();

    /* reset END */
    end_ = end_default_;

    create_token(CONTINUATION, HIDDEN_CHANNEL);

    lex_end();
    lex_begin();

    /* lex continuation */
    start_token();
    while (input_state_.char_position_in_line < continue_ && !eof() && *input_state_.next != '\n')
        consume();
    create_token(IGNORED, HIDDEN_CHANNEL);
}

/* if DOUBLE_BYTE_ENABLED check start of continuation for current line */
void lexer::check_continuation()
{
    // TODO: This is really inefficient if it works at all
    end_ = end_default_;

    const size_t relative = end_ - input_state_.char_position_in_line;
    const size_t avaliable = std::to_address(input_.end()) - input_state_.next;
    if (relative >= avaliable)
        return;
    const auto cc = input_state_.next[relative];
    if (cc == EOF_SYMBOL || cc == ' ')
        return;
    do
    {
        if (input_state_.next[end_ - 1] != cc)
            break;
        end_--;
    } while (end_ > begin_);
}

void lexer::lex_space()
{
    while (*input_state_.next == ' ' && before_end())
        consume();
    create_token(SPACE, DEFAULT_CHANNEL);
}

bool lexer::before_end() const
{
    return input_state_.char_position_in_line < end_
        || (unlimited_line_ && *input_state_.next != '\r' && *input_state_.next != '\n');
}

enum class character_type : unsigned char
{
    none = 0,
    identifier_divider = 0b00000001,
    space = 0b00000010,
    endline = 0b00000100,
    ord_char = 0b00001000,
    first_ord_char = 0b00010000,
    alpha = 0b00100000,
    number = 0b00100000,
    data_attr = 0b01000000,
};
constexpr character_type operator|(character_type l, character_type r)
{
    return static_cast<character_type>(static_cast<int>(l) | static_cast<int>(r));
}
constexpr character_type operator&(character_type l, character_type r)
{
    return static_cast<character_type>(static_cast<int>(l) & static_cast<int>(r));
}
constexpr character_type& operator|=(character_type& t, character_type s) { return t = t | s; }

constexpr const auto char_info = []() {
    std::array<character_type, 256> result {};

    using enum character_type;

    for (unsigned char c : { '*', '.', '-', '+', '=', '<', '>', ',', '(', ')', '\'', '/', '&', '|' })
        result[c] |= identifier_divider;
    result[' '] |= space;
    result['\r'] |= endline;
    result['\n'] |= endline;

    for (unsigned char c : std::string_view("0123456789$_#@abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"))
        result[c] |= ord_char;
    for (unsigned char c : std::string_view("$_#@abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"))
        result[c] |= first_ord_char;
    for (unsigned char c : std::string_view("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"))
        result[c] |= alpha;
    for (unsigned char c : std::string_view("0123456789"))
        result[c] |= number;
    for (unsigned char c : std::string_view("OSILTNKDosiltnkd"))
        result[c] |= data_attr;

    return result;
}();
constexpr character_type get_char_info(char32_t c)
{
    static_assert(char_info[0] == character_type::none);

    return char_info[c & -(c < char_info.size())];
}

bool lexer::ord_char(char_t c) noexcept
{
    return (get_char_info(c) & character_type::ord_char) != character_type::none;
}

bool lexer::ord_symbol(std::string_view symbol) noexcept
{
    using enum character_type;
    if (symbol.empty() || symbol.size() > 63 || (get_char_info(symbol.front()) & first_ord_char) == none)
        return false;

    for (unsigned char c : symbol)
        if ((get_char_info(c) & ord_char) == none)
            return false;

    return true;
}

void lexer::lex_word()
{
    using enum character_type;

    bool last_char_data_attr = false;
    auto ci = get_char_info(*input_state_.next);

    bool ord = (ci & first_ord_char) != none;
    bool num = (ci & number) != none;
    size_t last_part_ord_len = 0;
    size_t w_len = 0;
    bool last_ord = true;
    while ((ci & (space | endline | identifier_divider)) == none && !eof() && before_end())
    {
        bool curr_ord = (ci & ord_char) != none;
        if (!last_ord && curr_ord)
            break;

        last_part_ord_len = curr_ord ? last_part_ord_len + 1 : 0;
        ord &= curr_ord;

        num &= (*input_state_.next >= '0' && *input_state_.next <= '9');
        last_char_data_attr = (ci & data_attr) != none && w_len == 0;

        if (creating_var_symbol_ && !ord && w_len > 0 && w_len <= 63)
        {
            create_token(ORDSYMBOL);
            return;
        }

        consume();
        ci = get_char_info(*input_state_.next);

        ++w_len;
        last_ord = curr_ord;
    }

    bool var_sym_tmp = creating_var_symbol_;

    if (ord && w_len <= 63)
        create_token(ORDSYMBOL);
    else if (num)
        create_token(NUM);
    else
        create_token(IDENTIFIER);

    // We generate the ATTR token even when we created identifier, but it ends with exactly one ordinary symbol which is
    // also data attr symbol. That is because macro parameter "L'ORD must generate ATTR as string cannot start
    // with the apostrophe
    if (*input_state_.next == '\'' && last_char_data_attr && !var_sym_tmp && last_part_ord_len == 1
        && (unlimited_line_ || input_state_.char_position_in_line != end_))
    {
        start_token();
        consume();
        create_token(ATTR);
    }

    creating_attr_ref_ = !unlimited_line_ && input_state_.char_position_in_line == end_ && last_char_data_attr
        && !var_sym_tmp && w_len == 1;
}

bool lexer::set_begin(size_t begin)
{
    if (begin >= 1 && begin <= 40)
    {
        begin_ = begin;
        return true;
    }
    return false;
}

bool lexer::set_end(size_t end)
{
    if (end == 80)
        continuation_enabled_ = false;
    if (end >= 41 && end <= 80)
    {
        end_default_ = end;
        end_ = end_default_;
        return true;
    }
    return false;
}

bool lexer::set_continue(size_t cont)
{
    if (cont >= 2 && cont <= 40 && begin_ < cont)
    {
        continue_ = cont;
        return true;
    }
    return false;
}

void lexer::set_continuation_enabled(bool enabled) { continuation_enabled_ = enabled; }

bool lexer::is_process() const
{
    if (!process_allowed_)
        return false;
    for (const auto* p = input_state_.next; unsigned char c : std::string_view("*PROCESS"))
    {
        char_t next = *p++;
        if (next > (unsigned char)-1 || next == EOF_SYMBOL || c != utils::upper_cased[next])
            return false;
    }
    return true;
}

void lexer::set_ictl() { ictl_ = true; }

void lexer::lex_process()
{
    /* lex *PROCESS */
    start_token();
    for (size_t i = 0; i < 8; i++)
        consume();
    create_token(PROCESS);

    start_token();
    lex_space();

    size_t apostrophes = 0;
    end_++; /* including END column */
    while (!eof() && before_end() && *input_state_.next != '\n' && *input_state_.next != '\r'
        && (apostrophes % 2 == 1 || (apostrophes % 2 == 0 && *input_state_.next != ' ')))
    {
        start_token();
        lex_tokens();
    }
    end_--;
    lex_end();
}


std::string lexer::get_text(size_t start, size_t stop) const
{
    if (stop >= input_.size()) // EOF_SYMBOL
        return {};
    return utils::utf32_to_utf8(std::u32string_view(input_.data() + start, stop - start));
}
