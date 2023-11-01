/*
 * Copyright (c) 2021 Broadcom.
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

#include "op_code.h"

#include "context/instruction.h"

namespace hlasm_plugin::parser_library::processing {

inline unsigned char get_reladdr_bitmask(context::id_index id)
{
    if (id.empty())
        return 0;

    const auto [p_instr, p_mnemo] = context::instruction::find_machine_instruction_or_mnemonic(id.to_string_view());
    if (p_mnemo)
        return p_mnemo->reladdr_mask().mask();
    if (p_instr)
        return p_instr->reladdr_mask().mask();

    return 0;
}

// Generates value of L'* expression
unsigned char processing_status_cache_key::generate_loctr_len(std::string_view id)
{
    if (!id.empty())
    {
        if (const auto [mi, _] = context::instruction::find_machine_instruction_or_mnemonic(id); mi)
            return static_cast<unsigned char>(mi->size_in_bits() / 8);
    }
    return 1;
}

unsigned char processing_status_cache_key::generate_loctr_len(context::id_index id)
{
    return generate_loctr_len(id.empty() ? std::string_view() : id.to_string_view());
}

processing_status_cache_key::processing_status_cache_key(const processing_status& s)
    : form(s.first.form)
    , occurrence(s.first.occurrence)
    , is_alias(s.second.type == context::instruction_type::ASM && s.second.value.to_string_view() == "ALIAS")
    , loctr_len(
          s.second.type != context::instruction_type::MACH ? 1 : generate_loctr_len(s.second.value.to_string_view()))
    , rel_addr(s.second.type != context::instruction_type::MACH ? 0 : get_reladdr_bitmask(s.second.value))
{}
} // namespace hlasm_plugin::parser_library::processing
