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
	: mac_preproc[true]
	{
		$op = std::make_unique<macro_operand_string>($mac_preproc.collected_text,provider.get_range($mac_preproc.ctx));
	}
	|;

mac_op_o returns [operand_ptr op] 
	: mac_entry[true]?
	{
		if($mac_entry.ctx)
			$op = std::make_unique<macro_operand_chain>(std::move($mac_entry.chain),provider.get_range($mac_entry.ctx));
		else
			$op = std::make_unique<semantics::empty_operand>(provider.original_range);
	};

macro_ops returns [operand_list list] 
	: mac_op_o  {$list.push_back(std::move($mac_op_o.op));} (comma mac_op_o {$list.push_back(std::move($mac_op_o.op));})* EOF;



mac_preproc [bool top_level] returns [std::string collected_text]
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
		CONTINUATION?
		(
			ORDSYMBOL
			{
				auto name = $ORDSYMBOL->getText();
			}
			(
				CONTINUATION?
				(
					ORDSYMBOL
					{
						name += $ORDSYMBOL->getText();
					}
					|
					NUM
					{
						name += $NUM->getText();
					}
				)
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
			)*
			rpar												{$collected_text += ")";}
			|
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
			mac_preproc[false]									{$collected_text += $mac_preproc.collected_text;}
			(
				CONTINUATION?
				comma											{$collected_text += ",";}
			)+
			(
				SPACE
				remark CONTINUATION
			)?
		)*
		(
			mac_preproc[false]									{$collected_text += $mac_preproc.collected_text;}
		)?
		rpar													{$collected_text += ")";}
		|
		ap1=APOSTROPHE											{$collected_text += "'";}
		(
			mac_preproc_inner[$top_level]						{$collected_text += $mac_preproc_inner.collected_text;}
		)?
		ap2=(APOSTROPHE|ATTR)									{$collected_text += "'";}
		|
		attr													{$collected_text += "'";}
		(
			{!is_previous_attribute_consuming($top_level, _input->LT(-2))}?
			(
				mac_preproc_inner[$top_level]					{$collected_text += $mac_preproc_inner.collected_text;}
			)?
			ap2=(APOSTROPHE|ATTR)								{$collected_text += "'";}
		)?
		| CONTINUATION
	)+
	;
mac_preproc_inner [bool top_level] returns [std::string collected_text]
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
		| IDENTIFIER											{$collected_text += $IDENTIFIER.text;}
		| NUM													{$collected_text += $NUM.text;}
		| ORDSYMBOL												{$collected_text += $ORDSYMBOL.text;}
		| dot													{$collected_text += ".";}
		| AMPERSAND
		CONTINUATION?
		(
			ORDSYMBOL
			{
				auto name = $ORDSYMBOL->getText();
			}
			(
				CONTINUATION?
				(
					ORDSYMBOL
					{
						name += $ORDSYMBOL->getText();
					}
					|
					NUM
					{
						name += $NUM->getText();
					}
				)
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
			)*
			rpar												{$collected_text += ")";}
			|
			AMPERSAND											{$collected_text += "&&";}
		)
		| lpar													{$collected_text += "(";}
		| comma													{$collected_text += ",";}
		| rpar													{$collected_text += ")";}
		| (APOSTROPHE|ATTR) (APOSTROPHE|ATTR)					{$collected_text += "''";}
		| CONTINUATION
		| SPACE													{$collected_text += $SPACE.text;}
	)+
	;

mac_entry [bool top_level = true] returns [concat_chain chain]
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
			mac_entry[false]
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
			mac_entry[false]
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
		|
		ap1=APOSTROPHE
		{
			$chain.push_back(std::make_unique<char_str_conc>("'", provider.get_range($ap1)));
		}
		(
			string_ch_v
			{
				if (auto& p = $string_ch_v.point; p)
					$chain.push_back(std::move(p));
			}
		)*?
		ap2=(APOSTROPHE|ATTR)
		{
			$chain.push_back(std::make_unique<char_str_conc>("'", provider.get_range($ap2)));
			collector.add_hl_symbol(token_info(provider.get_range($ap1,$ap2),hl_scopes::string));
		}
		|
		ap1=ATTR
		{
			$chain.push_back(std::make_unique<char_str_conc>("'", provider.get_range($ap1)));
			bool attribute = true;
		}
		(
			{!is_previous_attribute_consuming($top_level, _input->LT(-2))}?
			(
				string_ch_v
				{
					if (auto& p = $string_ch_v.point; p)
						$chain.push_back(std::move(p));
				}
			)*?
			ap2=(APOSTROPHE|ATTR)
			{
				$chain.push_back(std::make_unique<char_str_conc>("'", provider.get_range($ap2)));
				collector.add_hl_symbol(token_info(provider.get_range($ap1,$ap2),hl_scopes::string));
				attribute = false;
			}
		)?
		{
			if (attribute)
				collector.add_hl_symbol(token_info(provider.get_range($ap1),hl_scopes::operator_symbol));
		}
	)+
	;
