/*
 * Copyright (c) 2025 Broadcom.
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

#ifndef HLASMPLUGIN_PARSERLIBRARY_WATCHER_REGISTRATION_PROVIDER_H
#define HLASMPLUGIN_PARSERLIBRARY_WATCHER_REGISTRATION_PROVIDER_H

#include <memory>
#include <string_view>

namespace hlasm_plugin::parser_library {

class watcher_registration_handle
{
    std::shared_ptr<const void> value;

public:
    explicit watcher_registration_handle() = default;
    explicit watcher_registration_handle(std::shared_ptr<const void> value) noexcept
        : value(std::move(value))
    {}
};

class watcher_registration_provider
{
protected:
    ~watcher_registration_provider() = default;

public:
    virtual watcher_registration_handle add_watcher(std::string_view uri, bool recursive) = 0;
};

} // namespace hlasm_plugin::parser_library
#endif
