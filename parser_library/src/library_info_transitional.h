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

#ifndef HLASMPLUGIN_PARSERLIBRARY_LIBRARY_INFO_TRANSITIONAL_H
#define HLASMPLUGIN_PARSERLIBRARY_LIBRARY_INFO_TRANSITIONAL_H

#include "library_info.h"

namespace hlasm_plugin::parser_library {
namespace context {
class hlasm_context;
} // namespace context
namespace workspaces {
class parse_lib_provider;
} // namespace workspaces

class library_info_transitional final : public library_info
{
    workspaces::parse_lib_provider* m_lib_provider;
    const context::hlasm_context* m_hlasm_ctx = nullptr;

    library_info_transitional(workspaces::parse_lib_provider& lib_provider)
        : m_lib_provider(&lib_provider)
    {}

public:
    bool has_library(std::string_view member) const override;

    library_info_transitional(workspaces::parse_lib_provider& lib_provider, const context::hlasm_context& hlasm_ctx)
        : m_lib_provider(&lib_provider)
        , m_hlasm_ctx(&hlasm_ctx)
    {}

    static const library_info_transitional empty;
};

} // namespace hlasm_plugin::parser_library

#endif
