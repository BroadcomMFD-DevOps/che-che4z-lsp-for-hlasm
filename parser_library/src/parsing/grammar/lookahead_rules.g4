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

 //rules for lookahead statement
parser grammar lookahead_rules; 

look_lab_instr  returns [std::optional<std::string> op_text, range op_range]
	: seq_symbol SPACE ORDSYMBOL (SPACE .*?)? EOF
	{
		collector.set_label_field($seq_symbol.ss,provider.get_range($seq_symbol.ctx));
		auto instr_id = parse_identifier($ORDSYMBOL->getText(),provider.get_range($ORDSYMBOL));
		collector.set_instruction_field(instr_id,provider.get_range($ORDSYMBOL));
		collector.set_operand_remark_field(provider.get_range($seq_symbol.ctx));
	}
	| seq_symbol .*? EOF
	{
		collector.set_label_field($seq_symbol.ss,provider.get_range($seq_symbol.ctx));
		collector.set_instruction_field(provider.get_range($seq_symbol.ctx));
		collector.set_operand_remark_field(provider.get_range($seq_symbol.ctx));
	}
	| ORDSYMBOL? SPACE instruction operand_field_rest EOF
	{
		if ($ORDSYMBOL)
		{
			auto r = provider.get_range($ORDSYMBOL);
			auto ord_symbol = $ORDSYMBOL->getText();
			auto id = add_id(ord_symbol);
			collector.set_label_field(id,std::move(ord_symbol),nullptr,r);
		}
		
		$op_text = $operand_field_rest.ctx->getText();
		$op_range = provider.get_range($operand_field_rest.ctx);
	}
	| bad_look EOF
	{
		collector.set_label_field(provider.get_range(_localctx));
		collector.set_instruction_field(provider.get_range(_localctx));
		collector.set_operand_remark_field(provider.get_range(_localctx));
	}
	| EOF;

bad_look
	: ~(ORDSYMBOL|DOT|SPACE) .*?
	| DOT ~(ORDSYMBOL) .*?
	| ORDSYMBOL ~(SPACE) .*?
	| ORDSYMBOL SPACE?
	| SPACE
	| DOT
	| ;

lookahead_operands_and_remarks_mach
	: SPACE+
	(
		lookahead_operand_list_mach
		{
			range r = provider.get_range($lookahead_operand_list_mach.ctx);
			collector.set_operand_remark_field(std::move($lookahead_operand_list_mach.operands), std::vector<range>(), r);
		}
		|
		EOF
		{
			range r = provider.get_range(_localctx);
			collector.set_operand_remark_field(operand_list(), std::vector<range>(), r);
		}
	)
	| EOF
	{
		range r = provider.get_range(_localctx);
		collector.set_operand_remark_field(operand_list(), std::vector<range>(), r);
	};

lookahead_operands_and_remarks_asm
	: SPACE+
	(
		lookahead_operand_list_asm
		{
			range r = provider.get_range($lookahead_operand_list_asm.ctx);
			collector.set_operand_remark_field(std::move($lookahead_operand_list_asm.operands), std::vector<range>(), r);
		}
		|
		EOF
		{
			range r = provider.get_range(_localctx);
			collector.set_operand_remark_field(operand_list(), std::vector<range>(), r);
		}
	)
	| EOF
	{
		range r = provider.get_range(_localctx);
		collector.set_operand_remark_field(operand_list(), std::vector<range>(), r);
	};

lookahead_operands_and_remarks_dat
	: SPACE+
	(
		data_def
		{
			operand_list operands;
			operands.push_back(std::make_unique<data_def_operand>(std::move($data_def.value),provider.get_range($data_def.ctx)));
			range r = provider.get_range($data_def.ctx);
			collector.set_operand_remark_field(std::move(operands), std::vector<range>(), r);
		}
		|
		EOF
		{
			range r = provider.get_range(_localctx);
			collector.set_operand_remark_field(operand_list(), std::vector<range>(), r);
		}
	)
	| EOF
	{
		range r = provider.get_range(_localctx);
		collector.set_operand_remark_field(operand_list(), std::vector<range>(), r);
	};

lookahead_operands_and_remarks_rest
	: SPACE* EOF
	{
		range r = provider.get_range(_localctx);
		collector.set_operand_remark_field(operand_list(), std::vector<range>(), r);
	};

lookahead_operand_list_mach returns [operand_list operands]
	: f=lookahead_operand_mach
	{
		{$operands.push_back(std::move($f.op));}
	}
	(
		COMMA n=lookahead_operand_mach
		{$operands.push_back(std::move($n.op));}
	)*;

lookahead_operand_mach returns [operand_ptr op]
	: mach_op
	{
		$op = std::move($mach_op.op);
	}
	|
	{$op = std::make_unique<semantics::empty_operand>(provider.get_empty_range( _localctx->getStart()));}
	;

lookahead_operand_list_asm returns [operand_list operands] locals [bool failed = false]
	: f=lookahead_operand_asm[&$failed]
	{
		{$operands.push_back(std::move($f.op));}
	}
	(
		COMMA n=lookahead_operand_asm[&$failed]
		{
			if($failed)
				$operands.push_back(std::make_unique<semantics::empty_operand>(provider.get_range($n.ctx)));
			else
				$operands.push_back(std::move($n.op));
		}
	)*;

lookahead_operand_asm[bool* failed] returns [operand_ptr op]
	:
	(
		asm_op
		{
			$op = std::move($asm_op.op);
		}
		|
		{$op = std::make_unique<semantics::empty_operand>(provider.get_empty_range( _localctx->getStart()));}
	)
	(
		(~(COMMA|SPACE|EOF))+
		{
			*$failed = true;
			$op = std::make_unique<semantics::empty_operand>(provider.get_empty_range( _localctx->getStart()));
		}
	)?
	;
