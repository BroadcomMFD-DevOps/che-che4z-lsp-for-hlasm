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

#include "logger.h"

#include <iostream>
#include <string>

#include "utils/time.h"

using namespace hlasm_plugin::language_server;

std::string current_time()
{
    auto t_o = hlasm_plugin::utils::timestamp::now();
    if (!t_o.has_value())
        return "<unknown time>";
    const auto& t = t_o.value();

    constexpr auto padded_append = [](auto& where, const auto& what, size_t pad) {
        auto s = std::to_string(what);
        if (s.size() < pad)
            where.append(pad - s.size(), '0');
        where.append(s);
    };

    std::string curr_time;
    curr_time.reserve(std::string_view("yyyy-MM-hh hh:mm:ss.uuuuuu").size());

    padded_append(curr_time, t.year(), 4);
    curr_time.append(1, '-');
    padded_append(curr_time, t.month(), 2);
    curr_time.append(1, '-');
    padded_append(curr_time, t.day(), 2);
    curr_time.append(1, ' ');
    padded_append(curr_time, t.hour(), 2);
    curr_time.append(1, ':');
    padded_append(curr_time, t.minute(), 2);
    curr_time.append(1, ':');
    padded_append(curr_time, t.second(), 2);
    curr_time.append(1, '.');
    padded_append(curr_time, t.microsecond(), 6);

    return curr_time;
}

void logger::log(std::string_view data)
{
    std::lock_guard g(mutex_);

    std::clog << current_time() << " " << data << '\n';
}
