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

#include "statement_fields.h"

#include "../expressions/conditional_assembly/terms/ca_var_sym.h"
#include "concatenation.h"
#include "utils/similar.h"

namespace hlasm_plugin::parser_library::semantics {

void label_si::resolve(diagnostic_op_consumer& diag)
{
    switch (type)
    {
        case label_si_type::ORD:
            break;
        case label_si_type::SEQ:
            break;
        case label_si_type::VAR:
            std::get<vs_ptr>(value)->resolve(context::SET_t_enum::C_TYPE, diag);
            can_have_undef_attr =
                expressions::ca_var_sym::estimate_undefined_attributd_symbols(std::get<vs_ptr>(value));
            break;
        case label_si_type::MAC:
            break;
        case label_si_type::CONC:
            for (const auto& c : std::get<concat_chain>(value))
                c.resolve(diag);
            can_have_undef_attr = false;
            for (auto& c : std::get<concat_chain>(value))
            {
                c.estimante_undef_attr();
                can_have_undef_attr |= c.can_have_undef_attr;
            }
            break;
        case label_si_type::EMPTY:
            break;
    }
}

void instruction_si::resolve(diagnostic_op_consumer& diag)
{
    switch (type)
    {
        case instruction_si_type::ORD:
            break;
        case instruction_si_type::CONC:
            for (const auto& c : std::get<concat_chain>(value))
                c.resolve(diag);
            can_have_undef_attr = false;
            for (auto& c : std::get<concat_chain>(this->value))
            {
                c.estimante_undef_attr();
                can_have_undef_attr |= c.can_have_undef_attr;
            }
            break;
        case instruction_si_type::EMPTY:
            break;
    }
}

bool literal_si_data::is_similar(const literal_si_data& other) const
{
    return utils::is_similar(get_dd(), other.get_dd());
}

} // namespace hlasm_plugin::parser_library::semantics
