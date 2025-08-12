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

// This file contains implementation of the data_def_type for
// these types: A, Y, S, R, V, Q, J

#include "checking/data_definition/data_def_type_base.h"
#include "checking/data_definition/data_definition_operand.h"
#include "diagnostic_op.h"

namespace hlasm_plugin::parser_library::checking {

//***************************   types A, Y   *****************************//

template<int min_byte_abs, int max_byte_abs, int min_byte_sym, int max_byte_sym, int min_bit, int max_bit>
nominal_diag_func check_A_AD_Y_length(const data_definition_common& common, bool all_absolute) noexcept
{
    // For absolute expressions, it is possible to specify bit or byte length modifier with bounds specified in
    // parameters.
    if (all_absolute)
    {
        if (common.length_in_bits && (common.length < min_bit || common.length > max_bit))
        {
            return [](const range& r, std::string_view type) {
                return diagnostic_op::error_D008(r, type, "bit length", min_bit, max_bit);
            };
        }
        else if (!common.length_in_bits && (common.length < min_byte_abs || common.length > max_byte_abs))
        {
            return [](const range& r, std::string_view type) {
                return diagnostic_op::error_D008(r, type, "length", min_byte_abs, max_byte_abs);
            };
        }
    }
    else
    { // For relocatable expressions, bit length is not allowed and byte has specific bounds.
        if (common.length_in_bits)
        {
            return [](const range& r, std::string_view type) {
                return diagnostic_op::error_D007(r, type, " with relocatable symbols");
            };
        }
        else if (!common.length_in_bits && (common.length < min_byte_sym || common.length > max_byte_sym))
        {
            return [](const range& r, std::string_view type) {
                return diagnostic_op::error_D008(
                    r, type, "length", min_byte_sym, max_byte_sym, " with relocatable symbols");
            };
        }
    }
    return nullptr;
}

nominal_diag_func check_A_length(const data_definition_common& common, bool all_absolute) noexcept
{
    return check_A_AD_Y_length<1, 8, 2, 4, 1, 128>(common, all_absolute);
}

nominal_diag_func check_AD_length(const data_definition_common& common, bool all_absolute) noexcept
{
    return check_A_AD_Y_length<1, 8, 2, 8, 1, 128>(common, all_absolute);
}

nominal_diag_func check_Y_length(const data_definition_common& common, bool all_absolute) noexcept
{
    return check_A_AD_Y_length<1, 2, 2, 2, 1, 16>(common, all_absolute);
}

} // namespace hlasm_plugin::parser_library::checking
