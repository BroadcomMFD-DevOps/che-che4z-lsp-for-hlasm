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

#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"

#include "../common_testing.h"
#include "empty_configs.h"
#include "file_manager_mock.h"
#include "utils/resource_location.h"
#include "utils/task.h"
#include "workspaces/file_manager_impl.h"
#include "workspaces/workspace_configuration.h"

using namespace ::testing;
using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::parser_library::workspaces;
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

TEST(workspace_configuration, refresh_needed_configs)
{
    NiceMock<file_manager_mock> fm;
    shared_json global_settings = make_empty_shared_json();

    EXPECT_CALL(fm, get_file_content(_))
        .WillRepeatedly(Invoke([](const auto&) -> hlasm_plugin::utils::value_task<std::optional<std::string>> {
            co_return std::nullopt;
        }));

    workspace_configuration cfg(fm, resource_location("test://workspace"), global_settings);

    EXPECT_TRUE(cfg.refresh_libraries({ resource_location("test://workspace/.hlasmplugin") }).run().value());
    EXPECT_TRUE(
        cfg.refresh_libraries({ resource_location("test://workspace/.hlasmplugin/proc_grps.json") }).run().value());
    EXPECT_TRUE(
        cfg.refresh_libraries({ resource_location("test://workspace/.hlasmplugin/pgm_conf.json") }).run().value());
    EXPECT_FALSE(cfg.refresh_libraries({ resource_location("test://workspace/something/else") }).run().value());
}

namespace {
class file_manager_refresh_needed_test : public file_manager_impl
{
public:
    hlasm_plugin::utils::value_task<std::optional<std::string>> get_file_content(
        const resource_location& location) override
    {
        using hlasm_plugin::utils::value_task;
        if (location.get_uri().ends_with("proc_grps.json"))
            return value_task<std::optional<std::string>>::from_value(m_file_proc_grps_content);
        else if (location.get_uri().ends_with("pgm_conf.json"))
            return value_task<std::optional<std::string>>::from_value(m_file_pgm_conf_content);
        else
            return value_task<std::optional<std::string>>::from_value(std::nullopt);
    }

private:
    const std::string m_file_proc_grps_content = R"({
    "pgroups": [
        {
            "name": "P1",
            "libs": [
                "test://workspace/externals/library1",
                "test://workspace/externals/library1/SYSTEM_LIBRARY",
                "test://workspace/externals/library2",
                "test://workspace/externals/library3"
            ]
        }
    ]
})";

    const std::string m_file_pgm_conf_content = R"({
  "pgms": [
    {
      "program": "pgm1",
      "pgroup": "P1"
    }
  ]

})";
};

class refresh_needed_test : public Test
{
public:
    void SetUp() override
    {
        EXPECT_EQ(m_cfg.parse_configuration_file().run().value(), parse_config_file_result::parsed);

        cache_content();
    }

    bool refresh_libs(const std::vector<resource_location>& uris)
    {
        auto refreshed_pgroups = m_cfg.refresh_libraries(uris).run().value();
        return refreshed_pgroups.has_value() && refreshed_pgroups->size() == 1;
    }

private:
    file_manager_refresh_needed_test m_fm;
    const resource_location ws_loc = resource_location("test://workspace/");
    const shared_json m_global_settings = make_empty_shared_json();
    workspace_configuration m_cfg = workspace_configuration(m_fm, ws_loc, m_global_settings);

    void cache_content()
    {
        auto pgm_p1 = m_cfg.get_program(resource_location("test://workspace/pgm1"));
        ASSERT_TRUE(pgm_p1);

        auto proc_grp = m_cfg.get_proc_grp_by_program(*pgm_p1);
        ASSERT_TRUE(proc_grp);

        auto libs = proc_grp->libraries();
        EXPECT_EQ(libs.size(), 4);

        for (size_t i = 0; i < 3; ++i)
            libs[i]->prefetch().run();
    }
};
} // namespace

TEST_F(refresh_needed_test, not_refreshed)
{
    // different scheme
    EXPECT_FALSE(refresh_libs({ resource_location("aaa://workspace/externals/library1") }));

    // different path
    EXPECT_FALSE(refresh_libs({ resource_location("test://workspace/something/else") }));

    // not used
    EXPECT_FALSE(refresh_libs({ resource_location("test://workspace/externals/library") }));
    EXPECT_FALSE(refresh_libs({ resource_location("test://workspace/externals/library/") }));
    EXPECT_FALSE(refresh_libs({ resource_location("test://workspace/externals/library4") }));
    EXPECT_FALSE(refresh_libs({ resource_location("test://workspace/externals/library4/") }));

    // not cached
    EXPECT_FALSE(refresh_libs({ resource_location("test://workspace/externals/library3") }));
}

TEST_F(refresh_needed_test, refreshed)
{
    EXPECT_TRUE(refresh_libs({ resource_location("test://workspace/externals/library1") }));
    EXPECT_TRUE(refresh_libs({ resource_location("test://workspace/externals/library1/") }));
    EXPECT_TRUE(refresh_libs({ resource_location("test://workspace/externals/library1/MAC") }));
    EXPECT_TRUE(refresh_libs({ resource_location("test://workspace/externals/library1/MAC/") }));
    EXPECT_TRUE(refresh_libs({ resource_location("test://workspace/externals/library1/mac") }));
    EXPECT_TRUE(refresh_libs({ resource_location("test://workspace/externals/library1/mac/") }));

    // whole tree gets deleted
    EXPECT_TRUE(refresh_libs({ resource_location("test://workspace") }));
    EXPECT_TRUE(refresh_libs({ resource_location("test://workspace/") }));
    EXPECT_TRUE(refresh_libs({ resource_location("test://workspace/externals") }));
    EXPECT_TRUE(refresh_libs({ resource_location("test://workspace/externals/") }));
    EXPECT_TRUE(refresh_libs({ resource_location("test://workspace/externals/library2") }));
    EXPECT_TRUE(refresh_libs({ resource_location("test://workspace/externals/library2/") }));
}
