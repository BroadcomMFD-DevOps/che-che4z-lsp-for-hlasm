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

#include <unordered_map>
#include <utility>
#include <vector>

#include "context/id_storage.h"
#include "statement_analyzer.h"
#include "utils/general_hashers.h"
#include "utils/resource_location.h"

namespace hlasm_plugin::parser_library::context {
class hlasm_context;
} // namespace hlasm_plugin::parser_library::context

namespace hlasm_plugin::parser_library::processing {
struct hit_count
{
    size_t count = 0;
    bool contains_statement = false;

    hit_count& operator+=(const hit_count& other_hc)
    {
        count += other_hc.count;
        contains_statement |= other_hc.contains_statement;
        return *this;
    }
};

struct hit_count_details
{
    std::vector<hit_count> line_hits;
    bool is_external_macro = true;
    size_t max_lineno = 0;

    hit_count_details()
        : line_hits(1000)
    {}

    static void merge(hit_count_details& to, const hit_count_details& from)
    {
        to.is_external_macro &= from.is_external_macro;
        to.max_lineno = std::max(to.max_lineno, from.max_lineno);

        auto& from_line_hits = from.line_hits;
        auto& to_line_hits = to.line_hits;

        if (from_line_hits.size() > to_line_hits.size())
            to_line_hits.resize(from_line_hits.size());

        for (size_t i = 0; i <= from.max_lineno; ++i)
            to_line_hits[i] += from_line_hits[i];
    }
};

using hit_count_map = std::
    unordered_map<utils::resource::resource_location, hit_count_details, utils::resource::resource_location_hasher>;

using stmt_lines_range = std::pair<size_t, size_t>;

class hit_count_analyzer final : public statement_analyzer
{
public:
    explicit hit_count_analyzer(context::hlasm_context& ctx);
    ~hit_count_analyzer() = default;

    void analyze(const context::hlasm_statement& statement,
        statement_provider_kind prov_kind,
        processing_kind proc_kind,
        bool) override;

    hit_count_map take_hit_counts() { return std::move(m_hit_counts); };

private:
    enum class statement_type
    {
        REGULAR,
        MACRO_INIT,
        MACRO_NAME,
        MACRO_BODY,
    };

    struct macro_header_definition_details
    {
        utils::resource::resource_location rl;
        stmt_lines_range init_line_r;
        stmt_lines_range name_line_r;
    };

    context::hlasm_context& m_ctx;
    hit_count_map m_hit_counts;
    statement_type m_stmt_type = statement_type::REGULAR;
    size_t m_macro_nest_level = 0;
    stmt_lines_range m_last_macro_init_line_ranges;
    std::unordered_map<context::id_index, macro_header_definition_details> m_macro_header_definitions;

    void emplace_macro_header_definitions(const context::id_index& id);

    statement_type get_stmt_type(const context::hlasm_statement& statement);
};

} // namespace hlasm_plugin::parser_library::processing

#endif
