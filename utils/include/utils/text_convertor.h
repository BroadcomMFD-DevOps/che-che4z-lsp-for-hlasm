/*
 * Copyright (c) 2026 Broadcom.
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

#ifndef HLASMPARSER_UTILS_TEXT_CONVERTOR_H
#define HLASMPARSER_UTILS_TEXT_CONVERTOR_H

#include <string>
#include <string_view>

namespace hlasm_plugin::utils {

struct text_convertor
{
    virtual void from(std::string& dst, std::string_view src) const = 0;
    virtual void to(std::string& dst, std::string_view src) const = 0;

protected:
    ~text_convertor() = default;
};

} // namespace hlasm_plugin::utils

#endif
