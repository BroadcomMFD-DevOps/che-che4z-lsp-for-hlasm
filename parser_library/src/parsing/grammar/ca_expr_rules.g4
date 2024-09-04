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

 //rules for CA expressions, variable symbol, CA string
parser grammar ca_expr_rules;

expr_general returns [ca_expr_ptr ca_expr]
    :
    {
        std::vector<ca_expr_ptr> ca_exprs;
    }
    (
        NOT SPACE*
        {
            auto not_r = provider.get_range($NOT);
            collector.add_hl_symbol(token_info(not_r, hl_scopes::operand));
            ca_exprs.push_back(std::make_unique<ca_symbol>(parse_identifier($NOT->getText(), not_r), not_r));
        }
    )*
    expr
    {
        if (!ca_exprs.empty()) {
            ca_exprs.push_back(std::move($expr.ca_expr));
            $ca_expr = std::make_unique<ca_expr_list>(std::move(ca_exprs), range(ca_exprs.front()->expr_range.start, ca_exprs.back()->expr_range.end), false);

        }
        else
            $ca_expr = std::move($expr.ca_expr);
    }
    ;
    finally
    {if (!$ca_expr) $ca_expr = std::make_unique<ca_constant>(0, provider.get_range(_localctx));}

expr returns [ca_expr_ptr ca_expr]
    : begin=expr_s
    {
        $ca_expr = std::move($begin.ca_expr);
    }
    (
        (
            (plus|minus) next=expr_s
            {
                auto r = provider.get_range($begin.ctx->getStart(), $next.ctx->getStop());
                if ($plus.ctx)
                    $ca_expr = std::make_unique<ca_basic_binary_operator<ca_add>>(std::move($ca_expr), std::move($next.ca_expr), r);
                else
                    $ca_expr = std::make_unique<ca_basic_binary_operator<ca_sub>>(std::move($ca_expr), std::move($next.ca_expr), r);
                $plus.ctx = nullptr;
            }
        )+
        |
        (
            dot term
            {
                auto r = provider.get_range($begin.ctx->getStart(), $term.ctx->getStop());
                $ca_expr = std::make_unique<ca_basic_binary_operator<ca_conc>>(std::move($ca_expr), std::move($term.ca_expr), r);
            }
        )+
        |
        {[](auto t){ return t!=PLUS && t!=MINUS && t!=DOT;}(_input->LA(1))}?
    );
    finally
    {if (!$ca_expr) $ca_expr = std::make_unique<ca_constant>(0, provider.get_range(_localctx));}

expr_s returns [ca_expr_ptr ca_expr]
    : begin=term_c
    {
        $ca_expr = std::move($begin.ca_expr);
    }
    ((slash|asterisk) next=term_c
    {
        auto r = provider.get_range($begin.ctx->getStart(), $next.ctx->getStop());
        if ($slash.ctx)
            $ca_expr = std::make_unique<ca_basic_binary_operator<ca_div>>(std::move($ca_expr), std::move($next.ca_expr), r);
        else
            $ca_expr = std::make_unique<ca_basic_binary_operator<ca_mul>>(std::move($ca_expr), std::move($next.ca_expr), r);
        $slash.ctx = nullptr;
    }
    )*;
    finally
    {if (!$ca_expr) $ca_expr = std::make_unique<ca_constant>(0, provider.get_range(_localctx));}

term_c returns [ca_expr_ptr ca_expr]
    : term
    {
        $ca_expr = std::move($term.ca_expr);
    }
    | plus tmp=term_c
    {
        auto r = provider.get_range($plus.ctx->getStart(), $tmp.ctx->getStop());
        $ca_expr = std::make_unique<ca_plus_operator>(std::move($tmp.ca_expr), r);
    }
    | minus tmp=term_c
    {
        auto r = provider.get_range($minus.ctx->getStart(), $tmp.ctx->getStop());
        $ca_expr = std::make_unique<ca_minus_operator>(std::move($tmp.ca_expr), r);
    };
    finally
    {if (!$ca_expr) $ca_expr = std::make_unique<ca_constant>(0, provider.get_range(_localctx));}

term returns [ca_expr_ptr ca_expr] locals [std::string buffer]
    : AMPERSAND var_symbol_base[$AMPERSAND]
    {
        auto r = provider.get_range($AMPERSAND, $var_symbol_base.ctx->getStop());
        $ca_expr = std::make_unique<ca_var_sym>(std::move($var_symbol_base.vs), r);
    }
    | SINGLECHAR
    (
        data_attribute[$SINGLECHAR]
        {
            auto r = provider.get_range($SINGLECHAR, $data_attribute.stop);
            auto val_range = $data_attribute.value_range;
            auto attr = $data_attribute.attribute;
            $ca_expr = std::visit([&r, &val_range, &attr](auto& v){
                return std::make_unique<ca_symbol_attribute>(std::move(v), attr, r, val_range);
            }, $data_attribute.value);
        }
        |
        {is_self_def()}? self_def_term[$SINGLECHAR]
        {
        auto r = provider.get_range($SINGLECHAR, $self_def_term.stop);
        $ca_expr = std::make_unique<ca_constant>($self_def_term.value, r);
        }
        |
        { $buffer = $SINGLECHAR->getText(); } (l=(IDENTIFIER|NUM|ORDSYMBOL|SINGLECHAR|NOT) {$buffer.append($l->getText());})*
        {
            auto r = provider.get_range($SINGLECHAR,$l?$l:$SINGLECHAR);
            auto name = parse_identifier(std::move($buffer), r);
        }
        subscript_ne?
        {
            if ($subscript_ne.ctx) {
                collector.add_hl_symbol(token_info(r,hl_scopes::operand));

                auto r = provider.get_range($SINGLECHAR, $subscript_ne.ctx->getStop());
                auto func = ca_common_expr_policy::get_function(name.to_string_view());
                $ca_expr = std::make_unique<ca_function>(name, func, std::move($subscript_ne.value), ca_expr_ptr{}, r);
            }
            else {
                collector.add_hl_symbol(token_info(r,hl_scopes::operand));
                $ca_expr = std::make_unique<ca_symbol>(name, r);
            }
        }
    )
    | sym=(ORDSYMBOL|NOT)
    { $buffer = $sym->getText(); } (l=(IDENTIFIER|NUM|ORDSYMBOL|SINGLECHAR|NOT) {$buffer.append($l->getText());})*
    {
        auto r = provider.get_range($sym,$l?$l:$sym);
        auto name = parse_identifier(std::move($buffer), r);
    }
    subscript_ne?
    {
        if ($subscript_ne.ctx) {
            collector.add_hl_symbol(token_info(r,hl_scopes::operand));

            auto r = provider.get_range($sym, $subscript_ne.ctx->getStop());
            auto func = ca_common_expr_policy::get_function(name.to_string_view());
            $ca_expr = std::make_unique<ca_function>(name, func, std::move($subscript_ne.value), ca_expr_ptr{}, r);
        }
        else {
            collector.add_hl_symbol(token_info(r,hl_scopes::operand));
            $ca_expr = std::make_unique<ca_symbol>(name, r);
        }
    }
    | signed_num
    {
        collector.add_hl_symbol(token_info(provider.get_range( $signed_num.ctx),hl_scopes::number));
        auto r = provider.get_range($signed_num.ctx);
        $ca_expr = std::make_unique<ca_constant>($signed_num.value, r);
    }
    | ca_string[nullptr, nullptr]
    {
        auto r = provider.get_range($ca_string.ctx);
        collector.add_hl_symbol(token_info(r, hl_scopes::string));
        $ca_expr = std::move($ca_string.ca_expr);
    }
    | lpar
    (
        expr_general rpar
        (
            id_no_dot subscript_ne
            {
                collector.add_hl_symbol(token_info(provider.get_range( $id_no_dot.ctx),hl_scopes::operand));

                auto r = provider.get_range($lpar.ctx->getStart(), $subscript_ne.ctx->getStop());
                auto func = ca_common_expr_policy::get_function($id_no_dot.name.to_string_view());
                $ca_expr = std::make_unique<ca_function>($id_no_dot.name, func, std::move($subscript_ne.value), std::move($expr_general.ca_expr), r);
            }
            | ca_string[$lpar.ctx, $expr_general.ctx]
            {
                auto r = provider.get_range($ca_string.ctx);
                collector.add_hl_symbol(token_info(r, hl_scopes::string));
                $ca_expr = std::move($ca_string.ca_expr);
            }
        )
        |
        SPACE* expr_space_c SPACE* rpar
        {
            auto r = provider.get_range($lpar.ctx->getStart(), $rpar.ctx->getStop());
            $ca_expr = std::make_unique<ca_expr_list>(std::move($expr_space_c.ca_exprs), r, true);
        }
    )
    ;
    finally
    {if (!$ca_expr) $ca_expr = std::make_unique<ca_constant>(0, provider.get_range(_localctx));}

signed_num returns [self_def_t value]
    : signed_num_ch                                    {$value = parse_self_def_term("D",get_context_text($signed_num_ch.ctx),provider.get_range($signed_num_ch.ctx));};

self_def_term [antlr4::Token* ord] returns [self_def_t value]
    : string
    {
        collector.add_hl_symbol(token_info(provider.get_range( $ord),hl_scopes::self_def_type));
        auto opt = $ord->getText();
        $value = parse_self_def_term(opt, $string.value, provider.get_range($ord,$string.ctx->getStop()));
    };

expr_list returns [ca_expr_ptr ca_expr]
    : lpar SPACE* expr_space_c SPACE* rpar
    {
        auto r = provider.get_range($lpar.ctx->getStart(), $rpar.ctx->getStop());
        $ca_expr = std::make_unique<ca_expr_list>(std::move($expr_space_c.ca_exprs), r, true);
    };
    finally
    {if (!$ca_expr) $ca_expr = std::make_unique<ca_constant>(0, provider.get_range(_localctx));}

expr_space_c returns [std::vector<ca_expr_ptr> ca_exprs]
    : expr
    {
        $ca_exprs.push_back(std::move($expr.ca_expr));
    }
    (
        SPACE* expr
        {
            $ca_exprs.push_back(std::move($expr.ca_expr));
        }
    )*
    ;

seq_symbol returns [seq_sym ss = seq_sym{}]
    : DOT id_no_dot
    {
        $ss = seq_sym{$id_no_dot.name,provider.get_range( $DOT, $id_no_dot.ctx->getStop())};
    };

subscript_ne returns [std::vector<ca_expr_ptr> value]
    : lpar SPACE? expr SPACE? rpar
    {
        $value.push_back(std::move($expr.ca_expr));
    }
    | lpar expr comma expr_comma_c rpar
    {
        $value.push_back(std::move($expr.ca_expr));
        $value.insert($value.end(), std::make_move_iterator($expr_comma_c.ca_exprs.begin()),std::make_move_iterator($expr_comma_c.ca_exprs.end()));
    };

subscript returns [std::vector<ca_expr_ptr> value]
    : lpar expr_comma_c rpar
    {
        $value = std::move($expr_comma_c.ca_exprs);
    }
    |
    {_input->LA(1) != LPAR }?
    ;


expr_comma_c returns [std::vector<ca_expr_ptr> ca_exprs]
    : expr
    {
        $ca_exprs.push_back(std::move($expr.ca_expr));
    }
    (
        comma expr
        {
            $ca_exprs.push_back(std::move($expr.ca_expr));
        }
    )*;

created_set_body returns [concat_chain concat_list]
    :
    (
        ORDSYMBOL
        {
            collector.add_hl_symbol(token_info(provider.get_range( $ORDSYMBOL),hl_scopes::var_symbol));
            $concat_list.emplace_back(char_str_conc($ORDSYMBOL->getText(), provider.get_range($ORDSYMBOL)));
        }
        |
        SINGLECHAR
        {
            collector.add_hl_symbol(token_info(provider.get_range( $SINGLECHAR),hl_scopes::var_symbol));
            $concat_list.emplace_back(char_str_conc($SINGLECHAR->getText(), provider.get_range($SINGLECHAR)));
        }
        |
        NOT
        {
            collector.add_hl_symbol(token_info(provider.get_range( $NOT),hl_scopes::var_symbol));
            $concat_list.emplace_back(char_str_conc($NOT->getText(), provider.get_range($NOT)));
        }
        |
        IDENTIFIER
        {
            collector.add_hl_symbol(token_info(provider.get_range( $IDENTIFIER),hl_scopes::var_symbol));
            $concat_list.emplace_back(char_str_conc($IDENTIFIER->getText(), provider.get_range($IDENTIFIER)));
        }
        |
        NUM
        {
            collector.add_hl_symbol(token_info(provider.get_range( $NUM),hl_scopes::var_symbol));
            $concat_list.emplace_back(char_str_conc($NUM->getText(), provider.get_range($NUM)));
        }
        |
        AMPERSAND var_symbol_base[$AMPERSAND]
        {
            $concat_list.emplace_back(var_sym_conc(std::move($var_symbol_base.vs)));
        }
        |
        dot
        {
            $concat_list.emplace_back(dot_conc(provider.get_range($dot.ctx)));
        }
    )+
    ;
    finally
    {concatenation_point::clear_concat_chain($concat_list);}

var_symbol_base [antlr4::Token* amp] returns [vs_ptr vs]
    :
    vs_id tmp=subscript
    {
        auto id = $vs_id.name;
        auto r = provider.get_range( $amp,$tmp.ctx->getStop());
        $vs = std::make_unique<basic_variable_symbol>(id, std::move($tmp.value), r);
        collector.add_hl_symbol(token_info(provider.get_range( $amp, $vs_id.ctx->getStop()),hl_scopes::var_symbol));
    }
    |
    lpar (clc=created_set_body)? rpar subscript
    {
        collector.add_hl_symbol(token_info(provider.get_range( $amp),hl_scopes::var_symbol));
        $vs = std::make_unique<created_variable_symbol>($clc.ctx ? std::move($clc.concat_list) : concat_chain{},std::move($subscript.value),provider.get_range($amp,$subscript.ctx->getStop()));
    }
    ;
    finally
    {if (!$vs) $vs = std::make_unique<basic_variable_symbol>(parse_identifier("", {}), std::vector<expressions::ca_expr_ptr>{}, provider.get_range($ctx));}

data_attribute[antlr4::Token* ord] returns [context::data_attr_kind attribute, std::variant<context::id_index, semantics::vs_ptr, semantics::literal_si> value, range value_range]
    : attr data_attribute_value
    {
        collector.add_hl_symbol(token_info(provider.get_range($ord), hl_scopes::data_attr_type));
        $attribute = get_attribute($ord->getText());
        $value = std::move($data_attribute_value.value);
        $value_range = provider.get_range( $data_attribute_value.ctx);
    };

data_attribute_value returns [std::variant<context::id_index, semantics::vs_ptr, semantics::literal_si> value]
    : literal
    {
        if (auto& lv = $literal.value; lv)
            $value = std::move(lv);
    }
    | AMPERSAND var_symbol_base[$AMPERSAND] DOT? // in reality, this seems to be much more complicated (arbitrary many dots are consumed for *some* attributes)
    {
        $value = std::move($var_symbol_base.vs);
    }
    | id
    {
        collector.add_hl_symbol(token_info(provider.get_range( $id.ctx), hl_scopes::ordinary_symbol));
        $value = $id.name;
    };

var_def returns [vs_ptr vs]
    : var_def_name var_def_substr
    {
        auto r = provider.get_range($var_def_name.ctx);
        if ($var_def_name.created_name.empty())
        {
            $vs = std::make_unique<basic_variable_symbol>($var_def_name.name, std::move($var_def_substr.value), r);
            collector.add_hl_symbol(token_info(r,hl_scopes::var_symbol));
        }
        else
            $vs = std::make_unique<created_variable_symbol>(std::move($var_def_name.created_name), std::move($var_def_substr.value), r);
    };

var_def_name returns [id_index name, concat_chain created_name]
    : AMPERSAND?
    (
        vs_id                                    {$name = $vs_id.name;}
        |
        lpar clc=created_set_body rpar            {$created_name = std::move($clc.concat_list);}
    );

var_def_substr returns [std::vector<ca_expr_ptr> value]
    : lpar num rpar
    {
        auto r = provider.get_range($num.ctx);
        $value.emplace_back(std::make_unique<ca_constant>($num.value, r));
    }
    |;

substring returns [expressions::ca_string::substring_t value]
    : lpar e1=expr_general comma (e2=expr_general|ASTERISK) rpar
    {
        $value.start = std::move($e1.ca_expr);
        $value.substring_range = provider.get_range($lpar.ctx->getStart(), $rpar.ctx->getStop());
        if ($e2.ctx)
        {
            $value.count = std::move($e2.ca_expr);
        }
    }
    |
    { _input->LA(1) != LPAR }?
    ;

ca_string [LparContext* l, Expr_generalContext* first_dupl]  returns [ca_expr_ptr ca_expr]
    :
    string_ch_v_c substring
    {
        auto r = provider.get_range($l ? $l->getStart() : $string_ch_v_c.ctx->getStart(), $substring.ctx->getStop());
        $ca_expr = std::make_unique<expressions::ca_string>(std::move($string_ch_v_c.chain), $first_dupl ? std::move($first_dupl->ca_expr) : ca_expr_ptr{}, std::move($substring.value), r);
    }
    (
        (lpar expr_general rpar)?
        string_ch_v_c substring
        {
            auto r = provider.get_range($lpar.ctx ? $lpar.ctx->getStart() : $string_ch_v_c.ctx->getStart(), $substring.ctx->getStop());
            auto next = std::make_unique<expressions::ca_string>(std::move($string_ch_v_c.chain), $expr_general.ctx ? std::move($expr_general.ca_expr) : ca_expr_ptr{}, std::move($substring.value), r);
            $ca_expr = std::make_unique<ca_basic_binary_operator<ca_conc>>(std::move($ca_expr), std::move(next), provider.get_range($ctx->getStart(), $substring.ctx->getStop()));
            $lpar.ctx = nullptr;

        }
    )*;
    finally
    {if (!$ca_expr) $ca_expr = std::make_unique<ca_constant>(0, provider.get_range(_localctx));}

string_ch_v_c returns [concat_chain chain]
    :
    (
        f=(APOSTROPHE|ATTR)
        {allow_ca_string()}?
        {
            if ($l) {
                $chain.emplace_back(char_str_conc("'", provider.get_range($l, $f)));
            }
        }
        ( ASTERISK                                  {$chain.emplace_back(char_str_conc("*", provider.get_range($ASTERISK)));}
        | MINUS                                     {$chain.emplace_back(char_str_conc("-", provider.get_range($MINUS)));}
        | PLUS                                      {$chain.emplace_back(char_str_conc("+", provider.get_range($PLUS)));}
        | LT                                        {$chain.emplace_back(char_str_conc("<", provider.get_range($LT)));}
        | GT                                        {$chain.emplace_back(char_str_conc(">", provider.get_range($GT)));}
        | SLASH                                     {$chain.emplace_back(char_str_conc("/", provider.get_range($SLASH)));}
        | EQUALS                                    {$chain.emplace_back(equals_conc(provider.get_range($EQUALS)));}
        | VERTICAL                                  {$chain.emplace_back(char_str_conc("|", provider.get_range($VERTICAL)));}
        | IDENTIFIER                                {$chain.emplace_back(char_str_conc($IDENTIFIER->getText(), provider.get_range($IDENTIFIER)));}
        | NUM                                       {$chain.emplace_back(char_str_conc($NUM->getText(), provider.get_range($NUM)));}
        | ORDSYMBOL                                 {$chain.emplace_back(char_str_conc($ORDSYMBOL->getText(), provider.get_range($ORDSYMBOL)));}
        | SINGLECHAR                                {$chain.emplace_back(char_str_conc($SINGLECHAR->getText(), provider.get_range($SINGLECHAR)));}
        | NOT                                       {$chain.emplace_back(char_str_conc($NOT->getText(), provider.get_range($NOT)));}
        | DOT                                       {$chain.emplace_back(dot_conc(provider.get_range($DOT)));}
        | l=AMPERSAND
        (
            r=AMPERSAND                             {$chain.emplace_back(char_str_conc("&&", provider.get_range($l,$r)));}
            |
            var_symbol_base[$l]                     {$chain.emplace_back(var_sym_conc(std::move($var_symbol_base.vs)));}
        )
        | COMMA                                     {$chain.emplace_back(char_str_conc(",", provider.get_range($COMMA)));}
        | LPAR                                      {$chain.emplace_back(char_str_conc("(", provider.get_range($LPAR)));}
        | RPAR                                      {$chain.emplace_back(char_str_conc(")", provider.get_range($RPAR)));}
        | SPACE                                     {$chain.emplace_back(char_str_conc($SPACE->getText(), provider.get_range($SPACE)));}
        )*
        l=(APOSTROPHE|ATTR)
    )+
    { [](auto t) { return t != APOSTROPHE && t != ATTR; }(_input->LA(1)) }?
    ;
    finally
    {concatenation_point::clear_concat_chain($chain);}
