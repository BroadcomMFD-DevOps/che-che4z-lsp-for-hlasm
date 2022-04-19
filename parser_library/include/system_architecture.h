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

#ifndef HLASMPARSER_PARSERLIBRARY_SYSTEM_ARCHITECTURE_H
#define HLASMPARSER_PARSERLIBRARY_SYSTEM_ARCHITECTURE_H

#include <string>

// This file system architecture versions
namespace hlasm_plugin::parser_library {
enum class system_architecture
{
    ZOP = 1,
    YOP,
    Z9,
    Z10,
    Z11,
    Z12,
    Z13,
    Z14,
    Z15,
    ESA,
    XA,
    _370,
    DOS,
    UNI
};

} // namespace hlasm_plugin::parser_library

#endif