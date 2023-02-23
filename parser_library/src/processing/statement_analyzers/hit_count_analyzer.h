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

#include <algorithm>
#include <optional>
#include <stack>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "context/id_storage.h"
#include "semantics/statement.h"
#include "statement_analyzer.h"
#include "utils/general_hashers.h"
#include "utils/resource_location.h"

namespace hlasm_plugin::parser_library::context {
class hlasm_context;
} // namespace hlasm_plugin::parser_library::context

namespace hlasm_plugin::parser_library::processing {
using stmt_lines_range = std::pair<size_t, size_t>;

struct line_detail
{
    size_t count = 0;
    bool contains_statement = false;
    std::unordered_set<context::id_index> macro_affiliation;

    line_detail& operator+=(const line_detail& other)
    {
        count += other.count;
        contains_statement |= other.contains_statement;
        macro_affiliation.insert(other.macro_affiliation.begin(), other.macro_affiliation.end());
        return *this;
    }

    line_detail& add(
        size_t other_count, bool other_contains_statement, const std::optional<context::id_index>& other_mac_id)
    {
        count += other_count;
        contains_statement |= other_contains_statement;
        if (other_mac_id)
            macro_affiliation.emplace(*other_mac_id);
        return *this;
    }
};

struct line_hits
{
    std::vector<line_detail> line_details;
    size_t max_lineno = 0;

    line_hits& merge(const line_hits& other)
    {
        max_lineno = std::max(max_lineno, other.max_lineno);

        if (!other.line_details.empty())
        {
            auto& other_line_hits = other.line_details;
            auto& our_line_hits = line_details;

            if (other_line_hits.size() > our_line_hits.size())
                our_line_hits.resize(other_line_hits.size());

            for (size_t i = 0; i <= other.max_lineno; ++i)
                our_line_hits[i] += other_line_hits[i];
        }

        return *this;
    }

    void add(
        size_t line, size_t count, bool contains_statement, const std::optional<context::id_index>& macro_affiliation)
    {
        update_max_and_resize(line);
        line_details[line].add(count, contains_statement, macro_affiliation);
    }

    void add(const stmt_lines_range& lines_range,
        size_t count,
        bool contains_statement,
        const std::optional<context::id_index>& macro_affiliation)
    {
        const auto& [start_line, end_line] = lines_range;

        update_max_and_resize(end_line);
        for (auto i = start_line; i <= end_line; ++i)
            line_details[i].add(count, contains_statement, macro_affiliation);
    }

private:
    void update_max_and_resize(size_t line)
    {
        max_lineno = std::max(max_lineno, line);

        if (line_details.size() <= line)
            line_details.resize(2 * line + 1000);
    }
};

struct hit_count_detail
{
    line_hits hits;
    bool has_sections = false;

    hit_count_detail& merge(const hit_count_detail& other)
    {
        has_sections |= other.has_sections;
        hits.merge(other.hits);

        return *this;
    }
};

struct macro_detail
{
    bool used = false;
    size_t macro_name_line;
    utils::resource::resource_location rl;

    macro_detail& merge(const macro_detail& other)
    {
        used |= other.used;
        macro_name_line = other.macro_name_line;
        rl = other.rl;

        return *this;
    }
};

struct hit_count
{
    std::unordered_map<utils::resource::resource_location, hit_count_detail, utils::resource::resource_location_hasher>
        hc_map;
    std::unordered_map<context::id_index, macro_detail> macro_details_map;
};

class hit_count_analyzer final : public statement_analyzer
{
public:
    explicit hit_count_analyzer(context::hlasm_context& ctx);
    ~hit_count_analyzer() = default;

    bool analyze(const context::hlasm_statement& statement,
        statement_provider_kind prov_kind,
        processing_kind proc_kind,
        bool) override;

    void analyze_aread_line(const utils::resource::resource_location& rl, size_t lineno, std::string_view) override;

    hit_count take_hit_count();

private:
    enum class statement_type
    {
        REGULAR,
        MACRO_INIT,
        MACRO_NAME,
        MACRO_BODY,
        MACRO_END
    };

    context::hlasm_context& m_ctx;
    hit_count m_hit_count;
    statement_type m_next_stmt_type = statement_type::REGULAR;
    std::stack<context::id_index> m_nest_macro_names;

    hit_count_detail& get_hc_detail_reference(const utils::resource::resource_location& rl);

    statement_type get_stmt_type(const semantics::core_statement& statement);
};

} // namespace hlasm_plugin::parser_library::processing

#endif
