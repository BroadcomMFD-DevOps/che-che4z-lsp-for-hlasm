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

#include "completion_item.h"

#include <algorithm>
#include <bitset>

#include "context/instruction.h"
#include "utils/concat.h"

namespace hlasm_plugin::parser_library::lsp {

struct operand_formatter
{
    std::string result;
    bool first = true;

    void start_operand()
    {
        if (first)
            first = false;
        else
            result.append(",");
    }
    operand_formatter& append(std::string_view s)
    {
        result.append(s);
        return *this;
    }
    operand_formatter& append(int i)
    {
        result.append(std::to_string(i));
        return *this;
    }
    operand_formatter& append_imm(unsigned short i)
    {
        if (i >= 0x100)
        {
            char buffer[std::numeric_limits<decltype(i)>::digits / 4 + 1];
            auto* end = buffer;
            while (i)
            {
                *end++ = "0123456789ABCDEF"[i & 0xf];
                i >>= 4;
            }
            std::reverse(buffer, end);
            result.append("X'").append(std::string_view(buffer, end)).append("'");
        }
        else if (i & 0x80)
        {
            result.append("X'80'");
            i &= ~0x80;
            if (i)
                result.append("+").append(std::to_string(i));
        }
        else
            result.append(std::to_string(i));

        return *this;
    }

    std::string take() { return std::move(result); }
};

completion_item_s::completion_item_s(std::string label,
    std::string detail,
    std::string insert_text,
    std::string documentation,
    completion_item_kind kind,
    bool snippet)
    : label(std::move(label))
    , detail(std::move(detail))
    , insert_text(std::move(insert_text))
    , documentation(std::move(documentation))
    , kind(kind)
    , snippet(snippet)
{}

namespace {
void process_machine_instruction(const context::machine_instruction& machine_instr,
    std::set<completion_item_s, completion_item_s::label_comparer>& items)
{
    operand_formatter detail(""); // operands used for hover - e.g. V,D12U(X,B)[,M]
    operand_formatter autocomplete(""); // operands used for autocomplete - e.g. V,D12U(X,B) [,M]

    int snippet_id = 1;
    bool first_optional = true;
    for (size_t i = 0; i < machine_instr.operands().size(); i++)
    {
        const auto& op = machine_instr.operands()[i];
        const bool is_optional = machine_instr.operands().size() - i <= machine_instr.optional_operand_count();
        if (is_optional && first_optional)
        {
            first_optional = false;
            autocomplete.append("${").append(snippet_id++).append(": [");
            detail.append("[");
        }
        autocomplete.start_operand();
        detail.start_operand();
        if (!is_optional)
        {
            detail.append(op.to_string(i + 1));
            autocomplete.append("${").append(snippet_id++).append(":").append(op.to_string(i + 1)).append("}");
        }
        else if (machine_instr.operands().size() - i > 1)
        {
            detail.append(op.to_string(i + 1)).append("[");
            autocomplete.append(op.to_string(i + 1)).append("[");
        }
        else
        {
            detail.append(op.to_string(i + 1)).append(std::string(machine_instr.optional_operand_count(), ']'));
            autocomplete.append(op.to_string(i + 1))
                .append(std::string(machine_instr.optional_operand_count(), ']'))
                .append("}");
        }
    }
    items.emplace(std::string(machine_instr.name()),
        "Operands: " + detail.take(),
        utils::concat(machine_instr.name(), " ${", snippet_id++, ":}", autocomplete.take()),
        utils::concat("Machine instruction \n\nInstruction format: ",
            context::instruction::mach_format_to_string(machine_instr.format())),
        completion_item_kind::mach_instr,
        true);
}

void process_assembler_instruction(const context::assembler_instruction& asm_instr,
    std::set<completion_item_s, completion_item_s::label_comparer>& items)
{
    items.emplace(std::string(asm_instr.name()),
        utils::concat(asm_instr.name(), "   ", asm_instr.description()),
        std::string(asm_instr.name()) + "   " /*+ description*/,
        "Assembler instruction",
        completion_item_kind::asm_instr);
}

void process_mnemonic_code(
    const context::mnemonic_code& mnemonic_instr, std::set<completion_item_s, completion_item_s::label_comparer>& items)
{
    operand_formatter subs_ops_mnems;
    operand_formatter subs_ops_nomnems;
    operand_formatter subs_ops_nomnems_no_snippets;

    // get mnemonic operands
    size_t iter_over_mnem = 0;
    int snippet_id = 1;
    bool first_optional = true;

    const auto& mach_operands = mnemonic_instr.instruction()->operands();
    const auto optional_count = mnemonic_instr.instruction()->optional_operand_count();

    auto replaces = mnemonic_instr.replaced_operands();

    std::bitset<context::machine_instruction::max_operand_count> ops_used_by_replacement;
    for (const auto& r : replaces)
        if (r.has_source())
            ops_used_by_replacement.set(r.source);

    for (size_t i = 0; i < mach_operands.size(); i++)
    {
        if (replaces.size() > iter_over_mnem)
        {
            auto replacement = replaces[iter_over_mnem];
            // can still replace mnemonics
            if (replacement.position == i)
            {
                ++iter_over_mnem;
                // replace current for mnemonic
                subs_ops_mnems.start_operand();
                switch (replacement.type)
                {
                    case context::mnemonic_replacement_kind::insert:
                        subs_ops_mnems.append_imm(replacement.value);
                        break;
                    case context::mnemonic_replacement_kind::copy:
                        break;
                    case context::mnemonic_replacement_kind::or_with:
                        subs_ops_mnems.append_imm(replacement.value).append("|");
                        break;
                    case context::mnemonic_replacement_kind::add_to:
                        subs_ops_mnems.append_imm(replacement.value).append("+");
                        break;
                    case context::mnemonic_replacement_kind::subtract_from:
                        subs_ops_mnems.append_imm(replacement.value).append("-");
                        break;
                }
                if (replacement.has_source())
                {
                    const auto op_string = mach_operands[replacement.source].to_string(1 + replacement.source);
                    subs_ops_mnems.append(op_string);
                    if (ops_used_by_replacement.test(replacement.source))
                    {
                        ops_used_by_replacement.reset(replacement.source);

                        subs_ops_nomnems.start_operand();
                        subs_ops_nomnems_no_snippets.start_operand();

                        subs_ops_nomnems.append("${").append(snippet_id++).append(":").append(op_string).append("}");
                        subs_ops_nomnems_no_snippets.append(op_string);
                    }
                }

                continue;
            }
        }
        ops_used_by_replacement.reset(i);

        const bool is_optional = mach_operands.size() - i <= optional_count;
        if (is_optional && first_optional)
        {
            first_optional = false;
            subs_ops_mnems.append(" [");
            subs_ops_nomnems.append("${").append(snippet_id++).append(": [");
            subs_ops_nomnems_no_snippets.append(" [");
        }

        subs_ops_mnems.start_operand();
        subs_ops_nomnems.start_operand();
        subs_ops_nomnems_no_snippets.start_operand();

        const auto op_string = mach_operands[i].to_string(i + 1);
        if (!is_optional)
        {
            subs_ops_mnems.append(op_string);
            subs_ops_nomnems.append("${").append(snippet_id++).append(":").append(op_string).append("}");
            subs_ops_nomnems_no_snippets.append(op_string);
        }
        else if (mach_operands.size() - i > 1)
        {
            subs_ops_mnems.append(op_string).append("[");
            subs_ops_nomnems.append(op_string).append("[");
            subs_ops_nomnems_no_snippets.append(op_string).append("[");
        }
        else
        {
            subs_ops_mnems.append(op_string).append(std::string(optional_count, ']'));
            subs_ops_nomnems.append(op_string).append(std::string(optional_count, ']')).append("}");
            subs_ops_nomnems_no_snippets.append(op_string).append(std::string(optional_count, ']'));
        }
    }
    items.emplace(std::string(mnemonic_instr.name()),
        "Operands: " + subs_ops_nomnems_no_snippets.take(),
        utils::concat(mnemonic_instr.name(), " ${", snippet_id++, ":}", subs_ops_nomnems.take()),
        utils::concat("Mnemonic code for ",
            mnemonic_instr.instruction()->name(),
            " instruction\n\nSubstituted operands: ",
            subs_ops_mnems.take(),
            "\n\nInstruction format: ",
            context::instruction::mach_format_to_string(mnemonic_instr.instruction()->format())),
        completion_item_kind::mach_instr,
        true);
}

void process_ca_instruction(
    const context::ca_instruction& ca_instr, std::set<completion_item_s, completion_item_s::label_comparer>& items)
{
    items.emplace(std::string(ca_instr.name()),
        "",
        std::string(ca_instr.name()),
        "Conditional Assembly",
        completion_item_kind::ca_instr);
}

} // namespace

const std::set<completion_item_s, completion_item_s::label_comparer> completion_item_s::m_instruction_completion_items =
    [] {
        std::set<completion_item_s, completion_item_s::label_comparer> result;

        for (const auto& instr : context::instruction::all_ca_instructions())
        {
            process_ca_instruction(instr, result);
        }

        for (const auto& instr : context::instruction::all_assembler_instructions())
        {
            process_assembler_instruction(instr, result);
        }

        for (const auto& instr : context::instruction::all_machine_instructions())
        {
            process_machine_instruction(instr, result);
        }

        for (const auto& instr : context::instruction::all_mnemonic_codes())
        {
            process_mnemonic_code(instr, result);
        }

        return result;
    }();

bool operator==(const completion_item_s& lhs, const completion_item_s& rhs)
{
    return lhs.label == rhs.label && lhs.detail == rhs.detail && lhs.insert_text == rhs.insert_text
        && lhs.documentation == rhs.documentation && lhs.kind == rhs.kind;
}

} // namespace hlasm_plugin::parser_library::lsp