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

lookahead_operand_field_rest returns [bool valid = false]
    : SPACE (~EOF)* {$valid=true;}
    | EOF {$valid=true;}
    |
    ;

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
            operands.push_back(std::make_unique<data_def_operand_inline>(std::move($data_def.value),provider.get_range($data_def.ctx)));
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

lookahead_operand_list_asm returns [operand_list operands] locals [bool failed = false]
    :
    {
        enable_lookahead_recovery();
    }
    f=lookahead_operand_asm[&$failed]
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
    finally
    {
        disable_lookahead_recovery();
    }

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
    ;
    catch[RecognitionException &e]
    {
        _errHandler->reportError(this, e);
        _localctx->exception = std::current_exception();

        if (auto t = _input->LA(1); t != COMMA && t != SPACE && t != EOF)
        {
            *_localctx->failed = true;
            _localctx->op = std::make_unique<semantics::empty_operand>(provider.get_empty_range(_localctx->getStart()));

            do {
                _input->consume();
                t = _input->LA(1);
            } while (t != COMMA && t != SPACE && t != EOF);
        }
    }
