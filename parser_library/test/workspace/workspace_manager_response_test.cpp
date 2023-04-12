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

#include "gtest/gtest.h"

#include "../workspace_manager_response_mock.h"
#include "workspace_manager_response.h"

using namespace hlasm_plugin::parser_library;
using namespace ::testing;

struct lifetime_mock : workspace_manager_response_mock<int>
{
    MOCK_METHOD(bool, destructor, (), ());

    ~lifetime_mock() { destructor(); }
};

TEST(workspace_manager_response, destructor_called)
{
    auto [p, impl] = make_workspace_manager_response(std::in_place_type<lifetime_mock>);

    EXPECT_CALL(*impl, destructor());
}

TEST(workspace_manager_response, copy)
{
    auto [p, impl] = make_workspace_manager_response(std::in_place_type<lifetime_mock>);

    {
        auto q = p;
    }

    EXPECT_CALL(*impl, destructor());
}

TEST(workspace_manager_response, move)
{
    auto [p, impl] = make_workspace_manager_response(std::in_place_type<lifetime_mock>);

    auto q = std::move(p);

    EXPECT_CALL(*impl, destructor());
}

TEST(workspace_manager_response, copy_assign)
{
    workspace_manager_response<int> q;
    lifetime_mock* impl;

    {
        auto [p, p_impl] = make_workspace_manager_response(std::in_place_type<lifetime_mock>);

        impl = p_impl;
        q = p;
    }

    EXPECT_CALL(*impl, destructor());
}

TEST(workspace_manager_response, move_assign)
{
    workspace_manager_response<int> q;
    lifetime_mock* impl;

    {
        auto [p, p_impl] = make_workspace_manager_response(std::in_place_type<lifetime_mock>);

        impl = p_impl;
        q = std::move(p);
    }

    EXPECT_CALL(*impl, destructor());
}

TEST(workspace_manager_response, provide)
{
    auto [p, impl] = make_workspace_manager_response(std::in_place_type<workspace_manager_response_mock<int>>);

    EXPECT_CALL(*impl, provide(5));

    p.provide(5);
}

TEST(workspace_manager_response, error)
{
    auto [p, impl] = make_workspace_manager_response(std::in_place_type<workspace_manager_response_mock<int>>);

    EXPECT_CALL(*impl, error(5, StrEq("Error message")));

    p.error(5, "Error message");
}

TEST(workspace_manager_response, invalidate_without_handler)
{
    auto [p, _impl] = make_workspace_manager_response(std::in_place_type<workspace_manager_response_mock<int>>);

    EXPECT_TRUE(p.valid());

    EXPECT_NO_FATAL_FAILURE(p.invalidate());

    EXPECT_FALSE(p.valid());
}
