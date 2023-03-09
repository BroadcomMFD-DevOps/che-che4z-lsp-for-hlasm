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

#ifndef PROCESSING_PROCESSING_FORMAT_H
#define PROCESSING_PROCESSING_FORMAT_H

#include <stdint.h>

namespace hlasm_plugin::parser_library::processing {

enum class processing_kind : uint8_t
{
    ORDINARY,
    LOOKAHEAD,
    MACRO,
    COPY
};

enum class processing_form : uint8_t
{
    MACH,
    ASM,
    MAC,
    CA,
    DAT,
    IGNORED,
    DEFERRED,
    UNKNOWN
};

enum class operand_occurrence : uint8_t
{
    PRESENT,
    ABSENT
};

// structure respresenting in which fashion should be statement processed
struct processing_format
{
    processing_format(
        processing_kind kind, processing_form form, operand_occurrence occurrence = operand_occurrence::PRESENT)
        : kind(kind)
        , form(form)
        , occurrence(occurrence)
    {}

    bool operator==(const processing_format& oth) const
    {
        return kind == oth.kind && form == oth.form && occurrence == oth.occurrence;
    }

    processing_kind kind;
    processing_form form;
    operand_occurrence occurrence;
};

} // namespace hlasm_plugin::parser_library::processing
#endif
