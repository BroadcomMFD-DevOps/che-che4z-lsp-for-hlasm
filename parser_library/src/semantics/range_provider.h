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

#ifndef SEMANTICS_RANGE_PROVIDER_H
#define SEMANTICS_RANGE_PROVIDER_H

#include <span>
#include <utility>

#include "range.h"

namespace hlasm_plugin::parser_library::semantics {

// state of range adjusting
enum class adjusting_state
{
    NONE,
    SUBSTITUTION,
    MODEL_REPARSE,
};

// class for computing range
class range_provider
{
    range original_range;
    std::span<const std::pair<std::pair<size_t, bool>, range>> model_substitutions;
    adjusting_state state;

    [[nodiscard]] position adjust_model_position(position pos, bool end) const noexcept;

public:
    explicit range_provider(range original_field_range, adjusting_state state);
    explicit range_provider(std::span<const std::pair<std::pair<size_t, bool>, range>> model_substitutions);
    explicit range_provider();

    [[nodiscard]] range adjust_range(range r) const noexcept;
    [[nodiscard]] const range& get_original_range() const noexcept { return original_range; }
};

} // namespace hlasm_plugin::parser_library::semantics
#endif
