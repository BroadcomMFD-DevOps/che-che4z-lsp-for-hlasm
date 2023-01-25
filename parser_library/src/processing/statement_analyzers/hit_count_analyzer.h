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

#include <functional>
#include <unordered_map>

#include "context/hlasm_context.h"
#include "protocol.h"
#include "statement_analyzer.h"
#include "utils/resource_location.h"

namespace hlasm_plugin::parser_library::processing {

struct hit_counts_entry
{
    utils::resource::resource_location rl;
    size_t line;
};

struct range_hasher
{
    std::size_t operator()(const range& r) const { return (r.start.line); }
};

struct hc_entry_hasher
{
    std::size_t operator()(const hit_counts_entry& hc_entry) const
    {
        return std::hash<std::string>()(hc_entry.rl.get_uri()) ^ (std::hash<size_t>()(hc_entry.line) << 1);

        // std::string file = hc_entry.rl.get_uri();
        // file.append(":").append(std::to_string(hc_entry.line));
        // return std::hash<std::string> {}(file);
    }
};

struct hc_entry_equal
{
    bool operator()(const hit_counts_entry& a, const hit_counts_entry& b) const
    {
        return a.rl == b.rl && a.line == b.line;
    }
};

class hit_count_analyzer final : public statement_analyzer
{
public:
    struct hit_count_details
    {
        range r;
        size_t count;
    };

    using hit_count_map = std::unordered_map<hit_counts_entry, hit_count_details, hc_entry_hasher, hc_entry_equal>;

    hit_count_analyzer(context::hlasm_context& ctx);
    ~hit_count_analyzer() = default;

    void analyze(const context::hlasm_statement& statement,
        statement_provider_kind prov_kind,
        processing_kind proc_kind,
        bool evaluated_model) override;

    const hit_count_map& get_hit_counts() const;

private:
    context::hlasm_context& m_ctx;
    hit_count_map m_hit_counts;
};

} // namespace hlasm_plugin::parser_library::processing

#endif