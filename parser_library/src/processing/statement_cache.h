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

#ifndef PROCESSING_STATEMENT_CACHE_H
#define PROCESSING_STATEMENT_CACHE_H

#include <forward_list>
#include <memory_resource>

#include "context/hlasm_statement.h"
#include "processing/op_code.h"

namespace hlasm_plugin::parser_library::processing {
struct statement_si_defer_done;
}

namespace hlasm_plugin::parser_library::processing {
class members_statement_provider;

// storage used to store one deferred statement in many parsed formats
// used by macro and copy definition to avoid multiple re-parsing of a deferred statements
class statement_cache_entry
{
    friend class statement_cache;
    friend class members_statement_provider;

    struct cached_statement_t;

    const cached_statement_t* cache_head = nullptr;
    context::shared_stmt_ptr base_stmt_;
    processing_status base_status;

public:
    statement_cache_entry(context::shared_stmt_ptr base, const processing_status& status) noexcept
        : base_stmt_(std::move(base))
        , base_status(status)
    {}

    const auto& get_base() const noexcept { return base_stmt_; }
    const auto& get_base_status() const noexcept { return base_status; }
};

class statement_cache : public std::enable_shared_from_this<statement_cache>
{
    friend class members_statement_provider;

    std::pmr::monotonic_buffer_resource mem_resource;
    std::pmr::forward_list<statement_cache_entry::cached_statement_t> cached_statements;
    std::vector<statement_cache_entry> entries;

public:
    auto begin() const { return entries.begin(); }
    auto end() const { return entries.end(); }
    auto size() const noexcept { return entries.size(); }
    auto empty() const noexcept { return entries.empty(); }
    const auto& front() const noexcept { return entries.front(); }
    const auto& back() const noexcept { return entries.back(); }
    const auto& get_base(size_t i) const { return entries.at(i).get_base(); }

    statement_cache(std::vector<std::pair<context::shared_stmt_ptr, processing_status>> stmts);
    statement_cache(const statement_cache&) = delete;
    statement_cache(statement_cache&&) = delete;
    statement_cache& operator=(const statement_cache&) = delete;
    statement_cache& operator=(statement_cache&&) = delete;
    ~statement_cache();
};

} // namespace hlasm_plugin::parser_library::processing
#endif
