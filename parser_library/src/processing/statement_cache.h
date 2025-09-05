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

#include "context/hlasm_statement.h"
#include "diagnostic_op.h"
#include "processing/op_code.h"

namespace hlasm_plugin::parser_library::processing {
struct statement_si_defer_done;
}

namespace hlasm_plugin::parser_library::processing {
class members_statement_provider;

// storage used to store one deferred statement in many parsed formats
// used by macro and copy definition to avoid multiple re-parsing of a deferred statements
class statement_cache
{
    friend class members_statement_provider;
    struct cached_statement_t
    {
        std::shared_ptr<processing::statement_si_defer_done> stmt;
        std::vector<diagnostic_op> diags;
    };
    // pair of processing format and reparsed statement
    // processing format serves as an identifier of reparsing kind
    using cache_t = std::pair<processing::processing_status_cache_key, cached_statement_t>;

    std::vector<cache_t> cache_;
    context::shared_stmt_ptr base_stmt_;
    processing_status base_status;

public:
    statement_cache(context::shared_stmt_ptr base, const processing_status& status) noexcept
        : base_stmt_(std::move(base))
        , base_status(status)
    {}

    const auto& get_base() const noexcept { return base_stmt_; }
    const auto& get_base_status() const noexcept { return base_status; }
};

} // namespace hlasm_plugin::parser_library::processing
#endif
