/*
 * Copyright (c) 2021 Broadcom.
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

#ifndef HLASMPARSER_PARSERLIBRARY_CONFIG_PROC_GRPS_H
#define HLASMPARSER_PARSERLIBRARY_CONFIG_PROC_GRPS_H

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "nlohmann/json_fwd.hpp"

namespace hlasm_plugin::parser_library::config {

struct library
{
    std::string path;
    std::vector<std::string> macro_extensions;
    bool optional = false;
};
void to_json(nlohmann::json& j, const library& p);
void from_json(const nlohmann::json& j, library& p);

struct assembler_options
{
    std::string sysparm;
    std::string profile;
    std::string optable;
    std::string system_id;

    bool optable_valid() const noexcept { return optable.size() == 0 || (optable.size() >= 2 && optable.size() <= 3); }


    bool valid() const noexcept { return sysparm.size() < 256 && optable_valid(); }
};
void to_json(nlohmann::json& j, const assembler_options& p);
void from_json(const nlohmann::json& j, assembler_options& p);

struct db2_preprocessor
{
    std::string version;

    bool valid() const noexcept { return version.size() <= 64; }

    friend bool operator==(const db2_preprocessor&, const db2_preprocessor&) = default;
};

void to_json(nlohmann::json& j, const db2_preprocessor& p);
void from_json(const nlohmann::json& j, db2_preprocessor& p);

struct cics_preprocessor
{
    bool prolog = true;
    bool epilog = true;
    bool leasm = false;

    bool valid() const noexcept { return true; }

    friend bool operator==(const cics_preprocessor&, const cics_preprocessor&) = default;
};

void to_json(nlohmann::json& j, const cics_preprocessor& p);
void from_json(const nlohmann::json& j, cics_preprocessor& p);

struct preprocessor_options
{
    std::variant<std::monostate, db2_preprocessor, cics_preprocessor> options;

    bool valid() const noexcept;

    friend bool operator==(const preprocessor_options&, const preprocessor_options&) = default;
};

struct processor_group
{
    std::string name;
    std::vector<library> libs;
    assembler_options asm_options;
    preprocessor_options preprocessor;
};
void to_json(nlohmann::json& j, const processor_group& p);
void from_json(const nlohmann::json& j, processor_group& p);

struct proc_grps
{
    std::vector<processor_group> pgroups;
    std::vector<std::string> macro_extensions;
};
void to_json(nlohmann::json& j, const proc_grps& p);
void from_json(const nlohmann::json& j, proc_grps& p);

} // namespace hlasm_plugin::parser_library::config

#endif // HLASMPARSER_PARSERLIBRARY_CONFIG_PROC_GRPS_H
