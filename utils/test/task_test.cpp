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

#include "utils/task.h"

using namespace hlasm_plugin::utils;

TEST(task, basics)
{
    struct test_data
    {
        int f = 0;
        int fail = 0;
        int g = 0;
        int h = 0;

        int excp = 0;
    };
    static constexpr auto f = [](test_data& data) -> task {
        data.f++;
        co_return;
    };
    static constexpr auto fail = [](test_data& data) -> task {
        data.fail++;
        static int i = 0;
        throw &i;
        co_return;
    };
    static constexpr auto g = [](test_data& data) -> task {
        data.g++;
        co_await f(data);
        try
        {
            co_await fail(data);
        }
        catch (int*)
        {
            data.excp++;
        }
        co_await f(data); // await suspend
    };

    static constexpr auto h = [](test_data& data) -> task {
        data.h++;
        co_await g(data);
        co_await f(data);
        co_await g(data);
    };

    test_data data;

    int resume_count = 0;
    for (auto x = h(data); !x.done();)
    {
        ++resume_count;
        x.resume();
    }

    EXPECT_EQ(resume_count, 1);
    EXPECT_EQ(data.f, 5);
    EXPECT_EQ(data.fail, 2);
    EXPECT_EQ(data.g, 2);
    EXPECT_EQ(data.h, 1);
    EXPECT_EQ(data.excp, 2);
}

TEST(task, basics_with_suspends)
{
    struct test_data
    {
        int f = 0;
        int fail = 0;
        int g = 0;
        int h = 0;

        int excp = 0;
    };
    static constexpr auto f = [](test_data& data) -> task {
        co_await task::suspend();
        data.f++;
        co_await task::suspend();
        co_return;
    };
    static constexpr auto fail = [](test_data& data) -> task {
        co_await task::suspend();
        data.fail++;
        co_await task::suspend();
        static int i = 0;
        co_await task::suspend();
        throw &i;
        co_await task::suspend();
        co_return;
    };
    static constexpr auto g = [](test_data& data) -> task {
        co_await task::suspend();
        data.g++;
        co_await f(data);
        try
        {
            co_await fail(data);
        }
        catch (int*)
        {
            data.excp++;
        }
        co_await f(data); // await suspend
    };

    static constexpr auto h = [](test_data& data) -> task {
        co_await task::suspend();
        data.h++;
        co_await g(data);
        co_await f(data);
        co_await g(data);
    };

    test_data data;

    int resume_count = 0;
    for (auto x = h(data); !x.done();)
    {
        ++resume_count;
        x.resume();
    }

    EXPECT_GT(resume_count, 1);
    EXPECT_EQ(data.f, 5);
    EXPECT_EQ(data.fail, 2);
    EXPECT_EQ(data.g, 2);
    EXPECT_EQ(data.h, 1);
    EXPECT_EQ(data.excp, 2);
}

TEST(task, excp_propagation)
{
    bool excp = false;

    static constexpr auto fail = []() -> task {
        throw 0;
        co_return;
    };
    static constexpr auto inner = []() -> task { co_await fail(); };
    static constexpr auto outer = []() -> task { co_await inner(); };
    static constexpr auto main = [](bool& e) -> task {
        try
        {
            co_await outer();
        }
        catch (int)
        {
            e = true;
        }
    };

    for (auto x = main(excp); !x.done();)
        x.resume();

    EXPECT_TRUE(excp);
}

TEST(task, direct_throw)
{
    static constexpr auto fail = []() -> task {
        throw 0;
        co_return;
    };

    auto x = fail();

    EXPECT_FALSE(x.done());
    EXPECT_ANY_THROW(x.resume());
    EXPECT_TRUE(x.done());
}

TEST(task, values)
{
    static constexpr auto stall = []() -> task { co_return; };
    static constexpr auto f1 = []() -> value_task<int> {
        co_await stall();
        co_return 1;
    };
    static constexpr auto f2 = []() -> value_task<int> {
        co_await stall();
        co_return 2;
    };
    static constexpr auto add = [](int v) -> value_task<int> {
        co_await stall();
        co_return v + co_await f1() + co_await f2();
    };

    auto x = add(3);

    int resume_count = 0;
    while (!x.done())
    {
        ++resume_count;
        x.resume();
    }

    EXPECT_EQ(resume_count, 1);
    EXPECT_EQ(x.value(), 6);
}

TEST(task, values_with_suspends)
{
    static constexpr auto stall = []() -> task {
        co_await task::suspend();
        co_return;
    };
    static constexpr auto f1 = []() -> value_task<int> {
        co_await stall();
        co_return 1;
    };
    static constexpr auto f2 = []() -> value_task<int> {
        co_await stall();
        co_return 2;
    };
    static constexpr auto add = [](int v) -> value_task<int> {
        co_await stall();
        co_return v + co_await f1() + co_await f2();
    };

    auto x = add(3);

    int resume_count = 0;
    while (!x.done())
    {
        ++resume_count;
        x.resume();
    }

    EXPECT_GT(resume_count, 1);
    EXPECT_EQ(x.value(), 6);
}

TEST(task, await_partially_started_coroutine)
{
    static constexpr auto f1 = []() -> value_task<int> {
        co_await task::suspend();
        co_await task::suspend();
        co_return 1;
    };
    static constexpr auto nested_suspend = [](bool& stop) -> task {
        stop = true;
        co_await task::suspend();
    };
    static constexpr auto inner = [](bool& stop) -> value_task<int> {
        co_await task::suspend();
        int a = co_await f1();

        co_await nested_suspend(stop);

        int b = co_await f1();
        co_await task::suspend();

        co_return a + b;
    };

    bool stop = false;
    auto i_task = inner(stop);

    while (!stop)
        i_task.resume();

    static constexpr auto outer = [](value_task<int> v) -> value_task<int> {
        int value = co_await std::move(v);
        co_return value + 1;
    };

    auto o_task = outer(std::move(i_task));

    EXPECT_EQ(o_task.run().value(), 3);
}

TEST(task, await_done_task)
{
    static constexpr auto inner = []() -> value_task<int> { co_return 1; };
    static constexpr auto outer = [](value_task<int> t) -> value_task<int> { co_return co_await std::move(t); };

    auto i_task = inner();
    i_task.resume();

    EXPECT_TRUE(i_task.done());

    EXPECT_EQ(outer(std::move(i_task)).run().value(), 1);
}
