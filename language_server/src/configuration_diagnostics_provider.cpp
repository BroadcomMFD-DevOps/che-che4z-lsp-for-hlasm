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

#include "configuration_diagnostics_provider.h"

#include <string_view>

#include "nlohmann/json.hpp"
#include "workspace_manager.h"

namespace {
std::string_view extract_method(const nlohmann::json& msg)
{
    auto method_it = msg.find("method");
    if (method_it == msg.end() || !method_it->is_string())
        return {};
    return method_it->get<std::string_view>();
}
} // namespace

namespace hlasm_plugin::language_server {

void configuration_diagnostics_provider::write(const nlohmann::json&)
{
    ws_mngr->toggle_non_critical_configuration_diagnostics();
}

void configuration_diagnostics_provider::write(nlohmann::json&& m) { write(m); }

message_router::message_predicate configuration_diagnostics_provider::get_filtering_predicate() const
{
    return [](const nlohmann::json& msg) {
        return extract_method(msg) == "toggle_non_critical_configuration_diagnostics";
    };
}

} // namespace hlasm_plugin::language_server
