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

#include "macro_param_variable.h"

#include <cstddef>
#include <stdexcept>

using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::parser_library::debugging;

macro_param_variable::macro_param_variable(const context::macro_param_base& param, std::vector<context::A_t> index)
    : macro_param_(param)
    , index_(std::move(index))
{
    if (!index_.empty())
    {
        name_ = std::to_string(index_.back());
        value_ = macro_param_.get_value(index_);
    }
    else
    {
        name_ = "&" + macro_param_.id.to_string();
        value_ = macro_param_.macro_param_base::get_value();
    }
}

const std::string& macro_param_variable::get_name() const { return name_; }

const std::string& macro_param_variable::get_value() const { return value_; }

set_type macro_param_variable::type() const { return set_type::C_TYPE; }

bool macro_param_variable::is_scalar() const { return !macro_param_.index_range(index_).has_value(); }

std::vector<variable_ptr> macro_param_variable::values() const
{
    std::vector<std::unique_ptr<variable>> vals;

    std::vector<context::A_t> child_index = index_;
    child_index.push_back(0);

    if (auto index_range = macro_param_.index_range(index_); index_range)
    {
        for (long long i = index_range->first; i <= index_range->second; ++i)
        {
            child_index.back() = (context::A_t)i;
            vals.push_back(std::make_unique<macro_param_variable>(macro_param_, child_index));
        }
    }
    return vals;
}

context::A_t macro_param_variable::size() const
{
    auto index_range = macro_param_.index_range(index_);
    if (!index_range)
        return 0;
    return index_range->second - index_range->first + 1; // TODO: overflow
}
