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

 //rules for macro operand
parser grammar macro_operand_rules; 


mac_op returns [operand_ptr op]
	: mac_preproc
	{
		$op = std::make_unique<macro_operand_string>($mac_preproc.collected_text,provider.get_range($mac_preproc.ctx));
	}
	|;

mac_op_o returns [operand_ptr op] 
	: mac_entry?
	{
		if($mac_entry.ctx)
			$op = std::make_unique<macro_operand_chain>(std::move($mac_entry.chain),provider.get_range($mac_entry.ctx));
		else
			$op = std::make_unique<semantics::empty_operand>(provider.original_range);
	};

macro_ops returns [operand_list list] 
	: mac_op_o  {$list.push_back(std::move($mac_op_o.op));} (comma mac_op_o {$list.push_back(std::move($mac_op_o.op));})* EOF;



mac_preproc returns [std::string collected_text]
	: 
	(
		asterisk												{$collected_text += "*";}
		| minus													{$collected_text += "-";}
		| plus													{$collected_text += "+";}
		| LT													{$collected_text += "<";}
		| GT													{$collected_text += ">";}
		| slash													{$collected_text += "/";}
		| equals												{$collected_text += "=";}
		| VERTICAL												{$collected_text += "|";}
		| IDENTIFIER											{$collected_text += $IDENTIFIER.text; collector.add_hl_symbol(token_info(provider.get_range($IDENTIFIER), hl_scopes::operand));}
		| NUM													{$collected_text += $NUM.text; collector.add_hl_symbol(token_info(provider.get_range($NUM), hl_scopes::operand));}
		| ORDSYMBOL												{$collected_text += $ORDSYMBOL.text; collector.add_hl_symbol(token_info(provider.get_range($ORDSYMBOL), hl_scopes::operand));}
		| dot													{$collected_text += ".";}
		| AMPERSAND
		(
			ORDSYMBOL
			{
				auto name = $ORDSYMBOL->getText();
			}
			(
				CONTINUATION?
				ORDSYMBOL
				{
					name += $ORDSYMBOL->getText();
				}
				|
				NUM
				{
					name += $NUM->getText();
				}
			)*
			{
				$collected_text += "&";
				$collected_text += name;
				collector.add_hl_symbol(token_info(provider.get_range($AMPERSAND,_input->LT(-1)),hl_scopes::var_symbol));
			}
			|
			lpar												{$collected_text += "&(";}
			(
				created_set_body								{$collected_text += $created_set_body.text;}
				|
				CONTINUATION
			)?
			rpar												{$collected_text += ")";}
			|
			CONTINUATION?
			AMPERSAND											{$collected_text += "&&";}
		)
		|
		lpar													{$collected_text += "(";}
		(
			comma												{$collected_text += ",";}
			|
			CONTINUATION
		)*
		(
			mac_preproc											{$collected_text += $mac_preproc.collected_text;}
			(
				comma											{$collected_text += ",";}
			)+
			(
				SPACE
				remark CONTINUATION
			)?
		)*
		(
			mac_preproc											{$collected_text += $mac_preproc.collected_text;}
		)?
		rpar													{$collected_text += ")";}
		| attr													{$collected_text += "'";}
		|
		ap1=APOSTROPHE											{$collected_text += "'";}
		(
			string_ch_v											{$collected_text += $string_ch_v.text;}
			|
			CONTINUATION
		)*?
		ap2=(APOSTROPHE|ATTR)									{$collected_text += "'";}
		| CONTINUATION
	)+
	;

mac_str_ch returns [concat_point_ptr point]
	: common_ch_v									{$point = std::move($common_ch_v.point);}
	| SPACE											{$point = std::make_unique<char_str_conc>($SPACE->getText(), provider.get_range($SPACE));}//here next are for deferred
	| LPAR											{$point = std::make_unique<char_str_conc>("(", provider.get_range($LPAR));}
	| RPAR											{$point = std::make_unique<char_str_conc>(")", provider.get_range($RPAR));}
	| COMMA											{$point = std::make_unique<char_str_conc>(",", provider.get_range($COMMA));};

mac_str_b returns [concat_chain chain]
	:
	| tmp=mac_str_b mac_str_ch						{$tmp.chain.push_back(std::move($mac_str_ch.point)); $chain = std::move($tmp.chain);};

mac_str returns [concat_chain chain]
	: ap1=APOSTROPHE mac_str_b ap2=(APOSTROPHE|ATTR)
	{
		$chain.push_back(std::make_unique<char_str_conc>("'", provider.get_range($ap1)));
		$chain.insert($chain.end(), std::make_move_iterator($mac_str_b.chain.begin()), std::make_move_iterator($mac_str_b.chain.end()));
		$chain.push_back(std::make_unique<char_str_conc>("'", provider.get_range($ap2)));
		collector.add_hl_symbol(token_info(provider.get_range($ap1,$ap2),hl_scopes::string)); 
	};

mac_ch returns [concat_chain chain]
	: common_ch_v
	{
		$chain.push_back(std::move($common_ch_v.point));
		auto token = $common_ch_v.ctx->getStart();
		if (token->getType() == lexing::lexer::Tokens::ORDSYMBOL && $common_ch_v.ctx->getStop()->getType() == lexing::lexer::Tokens::ORDSYMBOL)
			collector.add_hl_symbol(token_info(provider.get_range( $common_ch_v.ctx), hl_scopes::operand));
	}
	| mac_str										{$chain = std::move($mac_str.chain);}
	| mac_sublist									{$chain.push_back(std::move($mac_sublist.point));};

mac_ch_c returns [concat_chain chain]
	:
	| tmp=mac_ch_c mac_ch
	{
		$chain = std::move($tmp.chain);
		$chain.insert($chain.end(), std::make_move_iterator($mac_ch.chain.begin()), std::make_move_iterator($mac_ch.chain.end()));
	};

mac_entry_old returns [concat_chain chain]
	:
	ORDSYMBOL attr
	{
		collector.add_hl_symbol(token_info(provider.get_range($ORDSYMBOL), hl_scopes::data_attr_type));
		$chain.push_back(std::make_unique<char_str_conc>($ORDSYMBOL->getText(), provider.get_range($ORDSYMBOL)));
		$chain.push_back(std::make_unique<char_str_conc>("'", provider.get_range($attr.ctx)));
	}
	| mac_ch
	{
		$chain = std::move($mac_ch.chain);
	}
	| literal
	{
		$chain.push_back(std::make_unique<char_str_conc>($literal.ctx->getText(), provider.get_range($literal.ctx)));
	}
	| ORDSYMBOL EQUALS literal
	{
		collector.add_hl_symbol(token_info(provider.get_range($ORDSYMBOL), hl_scopes::operand));
		$chain.push_back(std::make_unique<char_str_conc>($ORDSYMBOL->getText(), provider.get_range($ORDSYMBOL)));
		$chain.push_back(std::make_unique<char_str_conc>("=", provider.get_range($EQUALS)));
		$chain.push_back(std::make_unique<char_str_conc>($literal.ctx->getText(), provider.get_range($literal.ctx)));
	}
	| tmp=mac_entry mac_ch
	{
		$chain = std::move($tmp.chain);
		$chain.insert($chain.end(), std::make_move_iterator($mac_ch.chain.begin()), std::make_move_iterator($mac_ch.chain.end()));
	};
	finally
	{concatenation_point::clear_concat_chain($chain);}

mac_sublist_b_c returns [concat_chain chain]
	: mac_ch_c										{$chain = std::move($mac_ch_c.chain);}
	| literal										{$chain.push_back(std::make_unique<char_str_conc>($literal.ctx->getText(), provider.get_range($literal.ctx)));};

mac_sublist_b returns [std::vector<concat_chain> chains]
	: mac_sublist_b_c										{$chains.push_back(std::move($mac_sublist_b_c.chain));}
	| tmp=mac_sublist_b comma mac_sublist_b_c			
	{
		$chains = std::move($tmp.chains);
		$chains.push_back(std::move($mac_sublist_b_c.chain));
	};

mac_sublist returns [concat_point_ptr point]
	: lpar mac_sublist_b rpar						{ $point = std::make_unique<sublist_conc>(std::move($mac_sublist_b.chains)); };

mac_entry returns [concat_chain chain]
	: 
	(
		asterisk		{$chain.push_back(std::make_unique<char_str_conc>("*", provider.get_range($asterisk.ctx)));}
		| minus			{$chain.push_back(std::make_unique<char_str_conc>("-", provider.get_range($minus.ctx)));}
		| plus			{$chain.push_back(std::make_unique<char_str_conc>("+", provider.get_range($plus.ctx)));}
		| LT			{$chain.push_back(std::make_unique<char_str_conc>("<", provider.get_range($LT)));}
		| GT			{$chain.push_back(std::make_unique<char_str_conc>(">", provider.get_range($GT)));}
		| slash			{$chain.push_back(std::make_unique<char_str_conc>("/", provider.get_range($slash.ctx)));}
		| equals		{$chain.push_back(std::make_unique<equals_conc>());}
		| VERTICAL		{$chain.push_back(std::make_unique<char_str_conc>("|", provider.get_range($VERTICAL)));}
		| IDENTIFIER	{$chain.push_back(std::make_unique<char_str_conc>($IDENTIFIER.text, provider.get_range($IDENTIFIER)));}
		| NUM			{$chain.push_back(std::make_unique<char_str_conc>($NUM.text, provider.get_range($NUM)));}
		| ORDSYMBOL
		{
			auto r = provider.get_range($ORDSYMBOL);
			$chain.push_back(std::make_unique<char_str_conc>($ORDSYMBOL.text, r));
			collector.add_hl_symbol(token_info(r, hl_scopes::operand));
		}
		| dot			{$chain.push_back(std::make_unique<dot_conc>());}
		| AMPERSAND
		(
			vs_id tmp=subscript
			{
				auto id = $vs_id.name;
				auto r = provider.get_range( $AMPERSAND,$tmp.ctx->getStop());
				$chain.push_back(std::make_unique<var_sym_conc>(std::make_unique<basic_variable_symbol>(id, std::move($tmp.value), r)));
			}
			|
			lpar (clc=created_set_body)? rpar subscript
			{
				$chain.push_back(std::make_unique<var_sym_conc>(std::make_unique<created_variable_symbol>($clc.ctx ? std::move($clc.concat_list) : concat_chain{},std::move($subscript.value),provider.get_range($AMPERSAND,$subscript.ctx->getStop()))));
			}
			|
			AMPERSAND
			{
				$chain.push_back(std::make_unique<char_str_conc>("&&", provider.get_range(_input->LT(-2),_input->LT(-1))));
			}
		)
		|
		lpar
		{
			std::vector<concat_chain> sublist;
			bool pending_empty = false;
		}
		(
			comma
			{
				sublist.emplace_back();
				pending_empty = true;
			}
		)*
		(
			mac_entry
			{
				sublist.push_back(std::move($mac_entry.chain));
			}
			comma
			(
				comma
				{
					sublist.emplace_back();
				}
			)*
			{
				pending_empty = true;
			}
		)*
		(
			mac_entry
			{
				sublist.push_back(std::move($mac_entry.chain));
				pending_empty = false;
			}
		)?
		{
			if (pending_empty)
			{
				sublist.emplace_back();
			}
			$chain.push_back(std::make_unique<sublist_conc>(std::move(sublist)));
		}
		rpar
		| attr		{$chain.push_back(std::make_unique<char_str_conc>("'", provider.get_range($attr.ctx)));}
		|
		ap1=APOSTROPHE
		{
			$chain.push_back(std::make_unique<char_str_conc>("'", provider.get_range($ap1)));
		}
		(
			string_ch_v
			{
				$chain.push_back(std::make_unique<char_str_conc>($string_ch_v.text, provider.get_range($string_ch_v.ctx)));
			}
		)*?
		ap2=(APOSTROPHE|ATTR)
		{
			$chain.push_back(std::make_unique<char_str_conc>("'", provider.get_range($ap2)));
			collector.add_hl_symbol(token_info(provider.get_range($ap1,$ap2),hl_scopes::string)); 
		}
	)+
	;
