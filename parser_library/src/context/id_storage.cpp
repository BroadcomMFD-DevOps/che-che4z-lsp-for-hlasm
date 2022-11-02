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

#include "id_storage.h"

#include "common_types.h"

using namespace hlasm_plugin::parser_library::context;

size_t id_storage::size() const { return lit_.size(); }

bool id_storage::empty() const { return lit_.empty(); }

std::optional<id_index> id_storage::find(std::string val) const
{
    if (val.empty())
        return id_index();

    to_upper(val);

    if (val.size() < id_index::buffer_size)
        return id_index(val);

    if (auto tmp = lit_.find(val); tmp != lit_.end())
        return id_index(&*tmp);
    else
        return std::nullopt;
}

id_index id_storage::add(std::string value)
{
    if (value.empty())
        return id_index();
    to_upper(value);

    if (value.size() < id_index::buffer_size)
        return id_index(value);

    return id_index(&*lit_.insert(std::move(value)).first);
}
