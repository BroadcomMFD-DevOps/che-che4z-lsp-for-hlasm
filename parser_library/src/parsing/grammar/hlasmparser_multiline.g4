/*
 * Copyright (c) 2022 Broadcom.
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

 //starting statement rules
 //rules for identifier, number, remark
parser grammar hlasmparser_multiline;

import
operand_field_rules,
lookahead_rules,
assembler_operand_rules,
model_operand_rules,
machine_expr_rules,
data_def_rules,
ca_expr_rules;

@header
{
    #include "lexing/token.h"
    #include "lexing/token_stream.h"
    #include "parsing/parser_impl.h"
    #include "expressions/conditional_assembly/ca_operator_unary.h"
    #include "expressions/conditional_assembly/ca_operator_binary.h"
    #include "expressions/conditional_assembly/terms/ca_constant.h"
    #include "expressions/conditional_assembly/terms/ca_expr_list.h"
    #include "expressions/conditional_assembly/terms/ca_function.h"
    #include "expressions/conditional_assembly/terms/ca_string.h"
    #include "expressions/conditional_assembly/terms/ca_symbol.h"
    #include "expressions/conditional_assembly/terms/ca_symbol_attribute.h"
    #include "expressions/conditional_assembly/terms/ca_var_sym.h"
    #include "expressions/mach_expr_term.h"
    #include "expressions/mach_operator.h"
    #include "expressions/data_definition.h"
    #include "semantics/operand_impls.h"
    #include "utils/string_operations.h"
    #include "utils/truth_table.h"

    namespace hlasm_plugin::parser_library::parsing
    {
        using namespace hlasm_plugin::parser_library;
        using namespace hlasm_plugin::parser_library::semantics;
        using namespace hlasm_plugin::parser_library::context;
        using namespace hlasm_plugin::parser_library::checking;
        using namespace hlasm_plugin::parser_library::expressions;
        using namespace hlasm_plugin::parser_library::processing;
    }

    /* disables unreferenced parameter (_localctx) warning */
    #ifdef _MSC_VER
        #pragma warning(push)
        #pragma warning(disable: 4100)
    #endif
}

@members {
    using parser_impl::initialize;
}

@footer
{
    #ifdef _MSC_VER
        #pragma warning(pop)
    #endif
}

options {
    tokenVocab = lex;
    superClass = parser_impl;
}

program : EOF;

num_ch
    : NUM+;

num returns [self_def_t value]
    : num_ch                                    {$value = parse_self_def_term("D",get_context_text($num_ch.ctx),provider.get_range($num_ch.ctx));};

signed_num_ch
    : MINUS? NUM+;

id returns [id_index name, id_index using_qualifier]
    : f=id_no_dot {$name = $f.name;} (dot s=id_no_dot {$name = $s.name; $using_qualifier = $f.name;})?;

id_no_dot returns [id_index name] locals [std::string buffer]
    : ORDSYMBOL { $buffer = $ORDSYMBOL->getText(); } (l=(IDENTIFIER|NUM|ORDSYMBOL) {$buffer.append($l->getText());})*
    {
        $name = parse_identifier(std::move($buffer),provider.get_range($ORDSYMBOL,$l?$l:$ORDSYMBOL));
    }
    ;

vs_id returns [id_index name]
    : ORDSYMBOL
    {
        std::string text = $ORDSYMBOL->getText();
        auto first = $ORDSYMBOL;
        auto last = first;
    }
    (
        NUM
        {
            text += $NUM->getText();
            last = $NUM;
        }
        |
        ORDSYMBOL
        {
            text += $ORDSYMBOL->getText();
            last = $ORDSYMBOL;
        }
    )*
    {
        $name = parse_identifier(std::move(text), provider.get_range(first, last));
    };

remark
    : (DOT|ASTERISK|MINUS|PLUS|LT|GT|COMMA|LPAR|RPAR|SLASH|EQUALS|AMPERSAND|APOSTROPHE|IDENTIFIER|NUM|VERTICAL|ORDSYMBOL|SPACE|ATTR)*;

remark_o returns [std::optional<range> value]
    : SPACE remark                            {$value = provider.get_range( $remark.ctx);}
    | ;

    //***** highlighting rules
comma
    : COMMA {collector->add_hl_symbol(token_info(provider.get_range( $COMMA),hl_scopes::operator_symbol)); };
dot
    : DOT {collector->add_hl_symbol(token_info(provider.get_range( $DOT),hl_scopes::operator_symbol)); };
apostrophe
    : APOSTROPHE;
attr
    : ATTR {collector->add_hl_symbol(token_info(provider.get_range( $ATTR),hl_scopes::operator_symbol)); };
lpar
    : LPAR { collector->add_hl_symbol(token_info(provider.get_range( $LPAR),hl_scopes::operator_symbol)); };
rpar
    : RPAR {collector->add_hl_symbol(token_info(provider.get_range( $RPAR),hl_scopes::operator_symbol)); };
ampersand
    : AMPERSAND { collector->add_hl_symbol(token_info(provider.get_range( $AMPERSAND),hl_scopes::operator_symbol)); };
equals
    : EQUALS { collector->add_hl_symbol(token_info(provider.get_range( $EQUALS),hl_scopes::operator_symbol)); };
asterisk
    : ASTERISK {collector->add_hl_symbol(token_info(provider.get_range( $ASTERISK),hl_scopes::operator_symbol)); };
slash
    : SLASH { collector->add_hl_symbol(token_info(provider.get_range( $SLASH),hl_scopes::operator_symbol)); };
minus
    : MINUS {collector->add_hl_symbol(token_info(provider.get_range( $MINUS),hl_scopes::operator_symbol)); };
plus
    : PLUS {collector->add_hl_symbol(token_info(provider.get_range( $PLUS),hl_scopes::operator_symbol)); };

