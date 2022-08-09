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

#include "parser_error_listener.h"

#include "lexing/token_stream.h"

enum Tokens
{
#include "grammar/lex.tokens"
};

using namespace hlasm_plugin::parser_library::lexing;

namespace hlasm_plugin::parser_library::parsing {
namespace {
bool is_comparative_sign(size_t input)
{
    return (input == LT || input == GT || input == EQUALS || input == EQ || input == OR || input == AND || input == LE
        || input == LTx || input == GTx || input == GE || input == NE);
}

bool is_sign(size_t input)
{
    return (input == ASTERISK || input == MINUS || input == PLUS || is_comparative_sign(input) || input == SLASH);
}

// return last symbol before eolln in line
int get_end_index(antlr4::TokenStream* input_stream, int start)
{
    if (start < (int)input_stream->size())
    {
        return (int)input_stream->size() - 1;
    }
    return -1;
}

bool can_follow_sign(size_t input)
{
    return (input == IDENTIFIER || input == ORDSYMBOL || input == AMPERSAND || input == LPAR || input == CONTINUATION
        || input == COMMENT);
}

bool can_be_before_sign(size_t input)
{
    return (input == IDENTIFIER || input == ORDSYMBOL || input == AMPERSAND || input == RPAR || input == CONTINUATION
        || input == COMMENT);
}

bool is_attribute_consuming(const antlr4::Token* token)
{
    if (!token)
        return false;

    auto text = token->getText();
    if (text.size() != 1)
        return false;

    auto c = std::toupper((unsigned char)text.front());

    return c == 'O' || c == 'S' || c == 'I' || c == 'L' || c == 'T';
}

bool can_consume(const antlr4::Token* token)
{
    if (!token)
        return false;

    auto text = token->getText();
    if (text.size() == 0)
        return false;

    auto c = std::toupper((unsigned char)text.front());

    return c == '=' || (c >= 'A' && c <= 'Z');
}

void iterate_error_stream(antlr4::TokenStream* input_stream,
    int start,
    int end,
    bool& right_prec,
    bool& only_par,
    bool& left_prec,
    bool& sign_followed,
    bool& sign_preceding,
    bool& unexpected_sign,
    bool& odd_apostrophes,
    bool& ampersand_followed)
{
    int parenthesis = 0;
    int apostrophes = 0;
    for (int i = start; i <= end; i++)
    {
        auto type = input_stream->get(i)->getType();
        if (type == LPAR)
            parenthesis--;
        else if (type == RPAR)
            parenthesis++;
        else
        {
            only_par = false;
            if ((is_sign(type) || type == AMPERSAND)
                && (i == end || (i < end && !can_follow_sign(input_stream->get(i + 1)->getType()))))
            {
                if (is_sign(type))
                    sign_followed = false;
                if (type == AMPERSAND)
                    ampersand_followed = false;
            }
            if ((is_sign(type) && type != PLUS && type != MINUS)
                && (i == start || (i != start && !can_be_before_sign(input_stream->get(i - 1)->getType()))))
                sign_preceding = false;
            if (is_comparative_sign(type))
                unexpected_sign = true;
            if (type == APOSTROPHE)
                apostrophes++;
            if (type == ATTR)
            {
                if (!is_attribute_consuming(input_stream->get(i - 1)) || !can_consume(input_stream->get(i + 1)))
                    apostrophes++;
            }
        }
        // if there is right bracket preceding left bracket
        if (parenthesis > 0)
            right_prec = true;
    }
    if (apostrophes % 2 == 1)
        odd_apostrophes = true;
    if (parenthesis < 0)
        left_prec = true;
}

bool is_expected(int exp_token, antlr4::misc::IntervalSet expectedTokens)
{
    return expectedTokens.contains(static_cast<size_t>(exp_token));
}
} // namespace

void parser_error_listener_base::syntaxError(
    antlr4::Recognizer*, antlr4::Token*, size_t line, size_t char_pos_in_line, const std::string&, std::exception_ptr e)
{
    try
    {
        if (e)
            std::rethrow_exception(e);
    }
    catch (antlr4::NoViableAltException& excp)
    {
        auto input_stream = dynamic_cast<token_stream*>(excp.getInputStream());

        auto expected_tokens = excp.getExpectedTokens();

        auto start_token = excp.getStartToken();
        int start_index = (int)start_token->getTokenIndex();

        auto alternate_start_index = [](auto ctx) {
            while (ctx)
            {
                auto first = ctx->getStart();
                if (!first)
                    break;

                if (first->getType() == antlr4::Token::EOF)
                {
                    ctx = dynamic_cast<const antlr4::ParserRuleContext*>(ctx->parent);
                    continue;
                }
                return (int)first->getTokenIndex();
            }
            return -1;
        }(dynamic_cast<const antlr4::ParserRuleContext*>(excp.getCtx()));

        // find first eoln

        if (alternate_start_index != -1 && alternate_start_index < start_index)
        {
            start_index = alternate_start_index;
        }

        auto first_symbol_type = input_stream->get(start_index)->getType();

        // while it's a space, skip spaces
        while (first_symbol_type == SPACE)
        {
            start_index++;
            first_symbol_type = input_stream->get(start_index)->getType();
        }

        auto end_index = get_end_index(input_stream, start_index);

        // no eolln, end index at last index of the stream
        if (end_index == -1)
        {
            end_index = (int)input_stream->size() - 1;
            // add_parser_diagnostic(range(position(line,char_pos_in_line)), diagnostic_severity::error, "S0004", "HLASM
            // plugin", "NO EOLLN - TO DO"); return;
        }

        bool sign_followed = true;
        bool sign_preceding = true;
        bool only_par = true;
        bool right_prec = false;
        bool left_prec = false;
        bool unexpected_sign = false;
        bool odd_apostrophes = false;
        bool ampersand_followed = true;

        iterate_error_stream(input_stream,
            start_index,
            end_index,
            right_prec,
            only_par,
            left_prec,
            sign_followed,
            sign_preceding,
            unexpected_sign,
            odd_apostrophes,
            ampersand_followed);

        // apostrophe expected
        if (odd_apostrophes && is_expected(APOSTROPHE, expected_tokens))
            add_parser_diagnostic(diagnostic_op::error_S0005, range(position(line, char_pos_in_line)));
        // right parenthesis has no left match
        else if (right_prec)
            add_parser_diagnostic(diagnostic_op::error_S0012, range(position(line, char_pos_in_line)));
        // left parenthesis has no right match
        else if (left_prec)
            add_parser_diagnostic(diagnostic_op::error_S0011, range(position(line, char_pos_in_line)));
        // nothing else but left and right parenthesis is present
        else if (only_par)
            add_parser_diagnostic(diagnostic_op::error_S0010, range(position(line, char_pos_in_line)));
        // sign followed by a wrong token
        else if (!sign_followed)
            add_parser_diagnostic(diagnostic_op::error_S0009, range(position(line, char_pos_in_line)));
        // ampersand not followed with a name of a variable symbol
        else if (!ampersand_followed)
            add_parser_diagnostic(diagnostic_op::error_S0008, range(position(line, char_pos_in_line)));
        // expression starting with a sign
        else if (!sign_preceding)
            add_parser_diagnostic(diagnostic_op::error_S0007, range(position(line, char_pos_in_line)));
        // unexpected sign in an expression - GT, LT etc
        else if (unexpected_sign)
            add_parser_diagnostic(diagnostic_op::error_S0006, range(position(line, char_pos_in_line)));
        // unfinished statement - solo label on line
        else if (start_token->getCharPositionInLine() == 0)
            add_parser_diagnostic(diagnostic_op::error_S0004, range(position(line, char_pos_in_line)));
        // other undeclared errors
        else
            add_parser_diagnostic(diagnostic_op::error_S0002, range(position(line, char_pos_in_line)));
    }
    catch (antlr4::InputMismatchException& excp)
    {
        auto offender = excp.getOffendingToken();

        if (offender->getType() == antlr4::Token::EOF)
            add_parser_diagnostic(diagnostic_op::error_S0003, range(position(line, char_pos_in_line)));
        else
            add_parser_diagnostic(diagnostic_op::error_S0002, range(position(line, char_pos_in_line)));
    }
    catch (...)
    {
        add_parser_diagnostic(diagnostic_op::error_S0001, range(position(line, char_pos_in_line)));
    }
}

void parser_error_listener_base::reportAmbiguity(
    antlr4::Parser*, const antlr4::dfa::DFA&, size_t, size_t, bool, const antlrcpp::BitSet&, antlr4::atn::ATNConfigSet*)
{}

void parser_error_listener_base::reportAttemptingFullContext(
    antlr4::Parser*, const antlr4::dfa::DFA&, size_t, size_t, const antlrcpp::BitSet&, antlr4::atn::ATNConfigSet*)
{}

void parser_error_listener_base::reportContextSensitivity(
    antlr4::Parser*, const antlr4::dfa::DFA&, size_t, size_t, size_t, antlr4::atn::ATNConfigSet*)
{}

} // namespace hlasm_plugin::parser_library::parsing
