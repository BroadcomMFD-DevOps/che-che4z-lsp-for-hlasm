/*
 * Copyright (c) 2023 Broadcom.
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

#ifndef PROCESSING_HIT_COUNT_ANALYZER_H
#define PROCESSING_HIT_COUNT_ANALYZER_H

#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "context/hlasm_context.h"
#include "protocol.h"
#include "statement_analyzer.h"
#include "utils/resource_location.h"

namespace hlasm_plugin::parser_library::processing {
struct hit_count_details
{
    range r;
    size_t count;
};

struct stmt_hit_count_details
{
    std::unordered_map<size_t, hit_count_details> stmt_hc_map;
    bool is_external_macro = true;
};

using hit_count_map = std::unordered_map<utils::resource::resource_location,
    stmt_hit_count_details,
    utils::resource::resource_location_hasher>;

class hit_count_analyzer final : public statement_analyzer
{
public:
    hit_count_analyzer(context::hlasm_context& ctx);
    ~hit_count_analyzer() = default;

    void analyze(const context::hlasm_statement& statement,
        statement_provider_kind prov_kind,
        processing_kind proc_kind,
        bool) override;

    inline hit_count_map&& take_hit_counts() { return std::move(m_hit_counts); };

private:
    enum class statement_type
    {
        REGULAR,
        MACRO_INIT,
        MACRO_NAME,
        MACRO_BODY,
    };

    context::hlasm_context& m_ctx;
    hit_count_map m_hit_counts;
    std::unordered_set<std::string_view> m_processed_members;
    statement_type m_stmt_type = statement_type::REGULAR;
    size_t m_in_macro_def = 0;
    statement_type get_stmt_type(const context::hlasm_statement& statement);
};

} // namespace hlasm_plugin::parser_library::processing

#endif