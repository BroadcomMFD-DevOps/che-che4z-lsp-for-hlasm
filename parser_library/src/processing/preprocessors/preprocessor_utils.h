/*
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

#ifndef HLASMPARSER_PARSERLIBRARY_PROCESSING_PREPROCESSOR_UTILS_H
#define HLASMPARSER_PARSERLIBRARY_PROCESSING_PREPROCESSOR_UTILS_H

#include <memory>
#include <optional>
#include <regex>
#include <vector>

#include "semantics/range_provider.h"
#include "semantics/statement.h"

namespace hlasm_plugin::parser_library::processing {

struct stmt_part_ids
{
    std::optional<size_t> label;
    std::vector<size_t> instruction;
    size_t operands;
    std::optional<size_t> remarks;
};

std::vector<semantics::preproc_details::name_range> get_operands_list(
    std::string_view operands, size_t column_offset, size_t lineno, const semantics::range_provider& rp);

template<typename PREPROC_STATEMENT, typename ITERATOR>
std::shared_ptr<PREPROC_STATEMENT> get_preproc_statement(
    const std::match_results<ITERATOR>& matches, stmt_part_ids ids, size_t lineno, size_t continuation_column = 15);

} // namespace hlasm_plugin::parser_library::processing

#endif