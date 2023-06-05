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

#include <type_traits>

#include "gtest/gtest.h"

#include "../common_testing.h"
#include "empty_configs.h"
#include "external_configuration_requests.h"
#include "file_manager_mock.h"
#include "workspaces/workspace_configuration.h"

using namespace ::testing;
using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::parser_library::workspaces;
using namespace hlasm_plugin::utils;
using namespace hlasm_plugin::utils::resource;

namespace {
template<int>
struct X
{
    char a;

    auto operator<=>(const X&) const = default;
};
} // namespace

TEST(workspace_configuration, library_options)
{
    static_assert(!std::is_copy_constructible_v<library_options>);

    constexpr auto eq = [](auto&& l, auto&& r) { return !(l < r || r < l); };

    X<0> x0_0 { 0 };
    library_options lx0_1(X<0> { 0 });
    EXPECT_TRUE(eq(lx0_1, x0_0));

    library_options lx0_2(X<0> { 0 });
    EXPECT_TRUE(eq(lx0_1, lx0_2));

    library_options lx0_moved_1(std::move(lx0_1));
    library_options lx0_moved_2(std::move(lx0_2));
    EXPECT_TRUE(eq(lx0_moved_1, lx0_moved_2));
    EXPECT_TRUE(eq(lx0_1, lx0_2));
    EXPECT_FALSE(eq(lx0_1, lx0_moved_1));

    X<0> x0_1 { 1 };
    EXPECT_TRUE(lx0_moved_1 < x0_1);
    EXPECT_TRUE(lx0_moved_1 < library_options(x0_1));

    X<1> x1_0 { 0 };
    EXPECT_FALSE(eq(lx0_moved_1, x1_0));
    EXPECT_FALSE(eq(lx0_moved_1, library_options(x1_0)));

    library_options lx1_1(x1_0);
    library_options lx1_2(x1_0);
    library_options lx1_3(x0_0);

    EXPECT_TRUE(eq(lx1_1, lx1_2));
    EXPECT_FALSE(eq(lx1_2, lx1_3));

    lx1_3 = library_options(x1_0);
    EXPECT_TRUE(eq(lx1_1, lx1_3));

    EXPECT_TRUE(library_options(X<2> { 1 }) < X<2> { 2 });
    EXPECT_TRUE(library_options(X<2> { 1 }) < library_options(X<2> { 2 }));
}

TEST(workspace_configuration, refresh_needed)
{
    NiceMock<file_manager_mock> fm;
    shared_json global_settings = make_empty_shared_json();

    EXPECT_CALL(fm, get_file_content(_))
        .WillRepeatedly(Invoke([](const auto&) -> hlasm_plugin::utils::value_task<std::optional<std::string>> {
            co_return std::nullopt;
        }));

    workspace_configuration cfg(fm, resource_location("test://workspace"), global_settings, nullptr);

    EXPECT_TRUE(cfg.refresh_libraries({ resource_location("test://workspace/.hlasmplugin") }).run().value());
    EXPECT_TRUE(
        cfg.refresh_libraries({ resource_location("test://workspace/.hlasmplugin/proc_grps.json") }).run().value());
    EXPECT_TRUE(
        cfg.refresh_libraries({ resource_location("test://workspace/.hlasmplugin/pgm_conf.json") }).run().value());
    EXPECT_FALSE(cfg.refresh_libraries({ resource_location("test://workspace/something/else") }).run().value());
}

namespace {
class external_configuration_requests_mock : public hlasm_plugin::parser_library::external_configuration_requests
{
public:
    MOCK_METHOD(void,
        read_external_configuration,
        (hlasm_plugin::parser_library::sequence<char> url,
            hlasm_plugin::parser_library::workspace_manager_response<hlasm_plugin::parser_library::sequence<char>>
                content),
        (override));
};
} // namespace

TEST(workspace_configuration, external_configurations_group_name)
{
    NiceMock<file_manager_mock> fm;
    shared_json global_settings = make_empty_shared_json();
    NiceMock<external_configuration_requests_mock> ext_confg;

    EXPECT_CALL(fm, get_file_content(resource_location("test://workspace/.hlasmplugin/proc_grps.json")))
        .WillOnce(Invoke([]() {
            return value_task<std::optional<std::string>>::from_value(R"(
{
  "pgroups": [
    {
      "name": "GRP1",
      "libs": [],
      "asm_options": {"SYSPARM": "PARM1"}
    }
  ]
}
)");
        }));
    EXPECT_CALL(fm, get_file_content(resource_location("test://workspace/.hlasmplugin/pgm_conf.json")))
        .WillOnce(Invoke([]() { return value_task<std::optional<std::string>>::from_value(std::nullopt); }));

    workspace_configuration cfg(fm, resource_location("test://workspace"), global_settings, &ext_confg);
    cfg.parse_configuration_file().run();

    EXPECT_CALL(ext_confg,
        read_external_configuration(
            Truly([](sequence<char> v) { return std::string_view(v) == "test://workspace/file1.hlasm"; }), _))
        .WillOnce(Invoke([](auto, auto channel) { channel.provide(sequence<char>(std::string_view(R"("GRP1")"))); }));

    const resource_location pgm_loc("test://workspace/file1.hlasm");

    cfg.load_alternative_config_if_needed(pgm_loc).run();

    const auto* pgm = cfg.get_program(pgm_loc);

    ASSERT_TRUE(pgm);
    EXPECT_TRUE(pgm->external);
    EXPECT_EQ(pgm->pgroup, proc_grp_id(basic_conf { "GRP1" }));

    const auto& grp = cfg.get_proc_grp(pgm->pgroup.value());

    asm_option opts;
    grp.apply_options_to(opts);
    EXPECT_EQ(opts, asm_option { .sysparm = "PARM1" });
}

TEST(workspace_configuration, external_configurations_group_inline)
{
    NiceMock<file_manager_mock> fm;
    shared_json global_settings = make_empty_shared_json();
    NiceMock<external_configuration_requests_mock> ext_confg;

    EXPECT_CALL(fm, get_file_content(_)).WillRepeatedly(Invoke([]() {
        return value_task<std::optional<std::string>>::from_value(std::nullopt);
    }));

    workspace_configuration cfg(fm, resource_location("test://workspace"), global_settings, &ext_confg);
    cfg.parse_configuration_file().run();

    EXPECT_CALL(ext_confg,
        read_external_configuration(
            Truly([](sequence<char> v) { return std::string_view(v) == "test://workspace/file1.hlasm"; }), _))
        .WillOnce(Invoke([](auto, auto channel) {
            channel.provide(sequence<char>(std::string_view(R"({
      "name": "GRP1",
      "libs": [
        "path"
      ],
      "asm_options": {"SYSPARM": "PARM1"}
    })")));
        }));

    const resource_location pgm_loc("test://workspace/file1.hlasm");

    cfg.load_alternative_config_if_needed(pgm_loc).run();

    const auto* pgm = cfg.get_program(pgm_loc);
    ASSERT_TRUE(pgm);
    EXPECT_TRUE(pgm->external);
    EXPECT_TRUE(std::holds_alternative<external_conf>(pgm->pgroup.value()));

    const auto& grp = cfg.get_proc_grp(pgm->pgroup.value());

    asm_option opts;
    grp.apply_options_to(opts);
    EXPECT_EQ(opts, asm_option { .sysparm = "PARM1" });
    EXPECT_EQ(grp.libraries().size(), 1);
}

TEST(workspace_configuration, external_configurations_prune)
{
    NiceMock<file_manager_mock> fm;
    shared_json global_settings = make_empty_shared_json();
    NiceMock<external_configuration_requests_mock> ext_confg;

    EXPECT_CALL(fm, get_file_content(_)).WillRepeatedly(Invoke([]() {
        return value_task<std::optional<std::string>>::from_value(std::nullopt);
    }));

    workspace_configuration cfg(fm, resource_location("test://workspace"), global_settings, &ext_confg);
    cfg.parse_configuration_file().run();

    static constexpr std::string_view grp_def(R"({
      "name": "GRP1",
      "libs": [
        "path"
      ],
      "asm_options": {"SYSPARM": "PARM1"}
    })");

    EXPECT_CALL(ext_confg,
        read_external_configuration(
            Truly([](sequence<char> v) { return std::string_view(v) == "test://workspace/file1.hlasm"; }), _))
        .WillOnce(Invoke([](auto, auto channel) { channel.provide(sequence<char>(grp_def)); }));

    const resource_location pgm_loc("test://workspace/file1.hlasm");

    cfg.load_alternative_config_if_needed(pgm_loc).run();

    cfg.prune_external_processor_groups(pgm_loc);

    const auto* pgm = cfg.get_program(pgm_loc);

    EXPECT_EQ(pgm, nullptr);

    EXPECT_THROW(cfg.get_proc_grp(external_conf { std::make_shared<std::string>(grp_def) }), std::out_of_range);
}

TEST(workspace_configuration, external_configurations_prune_all)
{
    NiceMock<file_manager_mock> fm;
    shared_json global_settings = make_empty_shared_json();
    NiceMock<external_configuration_requests_mock> ext_confg;

    EXPECT_CALL(fm, get_file_content(_)).WillRepeatedly(Invoke([]() {
        return value_task<std::optional<std::string>>::from_value(std::nullopt);
    }));

    workspace_configuration cfg(fm, resource_location("test://workspace"), global_settings, &ext_confg);
    cfg.parse_configuration_file().run();

    static constexpr std::string_view grp_def(R"({
      "name": "GRP1",
      "libs": [
        "path"
      ],
      "asm_options": {"SYSPARM": "PARM1"}
    })");

    EXPECT_CALL(ext_confg,
        read_external_configuration(
            Truly([](sequence<char> v) { return std::string_view(v) == "test://workspace/file1.hlasm"; }), _))
        .WillOnce(Invoke([](auto, auto channel) { channel.provide(sequence<char>(grp_def)); }));

    const resource_location pgm_loc("test://workspace/file1.hlasm");

    cfg.load_alternative_config_if_needed(pgm_loc).run();

    cfg.prune_external_processor_groups(resource_location());

    const auto* pgm = cfg.get_program(pgm_loc);

    EXPECT_EQ(pgm, nullptr);

    EXPECT_THROW(cfg.get_proc_grp(external_conf { std::make_shared<std::string>(grp_def) }), std::out_of_range);
}
