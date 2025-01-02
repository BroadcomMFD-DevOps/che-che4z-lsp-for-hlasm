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

#ifndef HLASMPLUGIN_PARSER_HLASMLEX_H
#define HLASMPLUGIN_PARSER_HLASMLEX_H

#include <string>
#include <vector>

#include "logical_line.h"
#include "range.h"
#include "token.h"

namespace hlasm_plugin::parser_library::lexing {
using char_t = char32_t;

struct u8string_view_with_newlines;

class lexer final
{
    void reset(position file_offset, size_t logical_column, bool process_allowed);

public:
    static constexpr char_t EOF_SYMBOL = (char_t)-1;

    struct stream_position
    {
        size_t line;
        size_t offset;
    };
    struct char_substitution
    {
        uint8_t server : 1;
        uint8_t client : 1;

        char_substitution& operator|=(const char_substitution& other)
        {
            server |= other.server;
            client |= other.client;
            return *this;
        }
    };
    explicit lexer();

    lexer(const lexer&) = delete;
    lexer(lexer&&) = delete;
    lexer& operator=(const lexer&) = delete;
    lexer& operator=(lexer&&) = delete;

    // resets lexer's state, goes to the source beginning
    char_substitution reset(
        u8string_view_with_newlines str, position file_offset, size_t logical_column, bool process_allowed = false);
    char_substitution reset(
        const logical_line<utils::utf8_iterator<std::string_view::iterator, utils::utf8_utf16_counter>>& l,
        position file_offset,
        size_t logical_column,
        bool process_allowed = false);

    // generates more tokens, main lexer logic
    bool more_tokens();
    size_t token_count() const noexcept { return tokens.size(); }
    token* get_token(size_t i) noexcept { return &tokens[i]; }
    std::string get_text(size_t start, size_t stop) const;

    enum Tokens : int
    {
#include "parsing/grammar/lex.tokens"
    };

    enum Channels : unsigned
    {
        DEFAULT_CHANNEL = 0,
        HIDDEN_CHANNEL = 1
    };

    // set continuation column
    bool set_continue(size_t cont);
    // enable ictl
    void set_ictl();

    [[nodiscard]] auto get_continuation_column() const noexcept { return continue_; }

    static bool ord_char(char_t c) noexcept;
    static bool ord_symbol(std::string_view symbol) noexcept;

    const std::vector<size_t>& get_line_limits() const { return line_limits; }

    [[nodiscard]] auto peek_initial_input_state() const noexcept
    {
        return std::pair(initial_input_state, input_.data());
    }
    [[nodiscard]] auto peek_input_size() const noexcept { return input_.size(); }

protected:
    // creates token and inserts to input stream
    void create_token(int ttype, unsigned channel = Channels::DEFAULT_CHANNEL);
    // consumes char from input & updates lexer state
    void consume();

private:
    bool creating_var_symbol_ = false;
    bool creating_attr_ref_ = false;
    bool process_allowed_ = false;

    size_t last_token_id_ = 0;

    std::vector<token> tokens;
    std::vector<std::vector<token>> retired_tokens;
    std::vector<size_t> line_limits;

    size_t begin_ = 0;
    size_t continue_ = 15;

    std::vector<char_t> input_;
    std::vector<size_t> newlines_;

    struct input_state
    {
        const char_t* next;
        const size_t* nl;
        size_t line = 0;
        size_t char_position_in_line = 0;
        size_t char_position_in_line_utf16 = 0;
    };

    input_state initial_input_state;
    input_state input_state_;

    // captures lexer state at the beginning of a token
    input_state token_start_state_;
    input_state last_line;

    bool eof() const;

    // captures lexer state at the beginning of a token
    void start_token();
    // lex whitespace
    void lex_space();
    // check if before end_ and EOL
    bool before_end() const;

    // lexes everything not lexed in lex_tokens()
    void lex_word();

    // lexes everything not lexed in nextToken()
    void lex_tokens();
    // lexes PROCESS instruction
    void lex_process();

    bool is_process() const;
};

} // namespace hlasm_plugin::parser_library::lexing
#endif
