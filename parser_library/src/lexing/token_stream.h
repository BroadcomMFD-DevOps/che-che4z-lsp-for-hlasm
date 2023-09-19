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

#ifndef HLASMPLUGIN_PARSER_LIBRARY_TOKENSTREAM_H
#define HLASMPLUGIN_PARSER_LIBRARY_TOKENSTREAM_H

#include <memory>
#include <utility>

#include "BufferedTokenStream.h"
#include "lexer.h"

namespace hlasm_plugin::parser_library::lexing {

// custom implementation of antlr token stream
// helps to control the filtering of the token stream
class token_stream final : public antlr4::BufferedTokenStream
{
    bool enabled_cont_;
    bool needSetup_;
    lexer* token_source;

public:
    explicit token_stream(lexer* token_source);

    // enable continuation token in the token stream
    void enable_continuation();
    // filter continuation token from the token stream
    void disable_continuation();

    antlr4::Token* LT(ssize_t k) override;

    std::string getText(const antlr4::misc::Interval& interval) override;

    void reset() override;
    // prepares this object to append more tokens
    void append();

    auto get_line_limits() const { return token_source->get_line_limits(); }

private:
    ssize_t adjustSeekIndex(size_t i) override;

    antlr4::Token* LB(size_t k) override;

    void setup() override;

    bool is_on_channel(antlr4::Token* token);

    size_t next_token_on_channel(size_t i);

    size_t previous_token_on_channel(size_t i);

    token* get_internal(size_t i);

    std::vector<decltype(_tokens)> tokens_;
};

} // namespace hlasm_plugin::parser_library::lexing
#endif
