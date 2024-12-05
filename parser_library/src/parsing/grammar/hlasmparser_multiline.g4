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
