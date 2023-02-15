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

#ifndef HLASMPLUGIN_UTILS_CONCAT_H
#define HLASMPLUGIN_UTILS_CONCAT_H

#include <cassert>
#include <coroutine>
#include <utility>

namespace hlasm_plugin::utils {

struct task
{
    class awaiter;
    struct promise_type
    {
        task get_return_object() { return { std::coroutine_handle<promise_type>::from_promise(*this) }; }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept
        {
            detach();
            return {};
        }
        void return_void() {}
        void unhandled_exception() { throw; }

        std::coroutine_handle<promise_type> next_step = std::coroutine_handle<promise_type>::from_promise(*this);
        awaiter* active = nullptr;
        std::coroutine_handle<promise_type> top_waiter = std::coroutine_handle<promise_type>::from_promise(*this);

        std::coroutine_handle<promise_type> attach(std::coroutine_handle<promise_type> current_top_waiter, awaiter* a)
        {
            auto to_resume = std::exchange(
                current_top_waiter.promise().next_step, std::coroutine_handle<promise_type>::from_promise(*this));
            active = a;
            top_waiter = current_top_waiter;

            return to_resume;
        }

        void detach() noexcept
        {
            if (active)
            {
                top_waiter.promise().next_step = active->to_resume;

                next_step = std::coroutine_handle<promise_type>::from_promise(*this);
                active = nullptr;
                top_waiter = std::coroutine_handle<promise_type>::from_promise(*this);
            }
        }
    };

    class awaiter
    {
        promise_type& self;
        std::coroutine_handle<promise_type> to_resume {};

        friend struct promise_type;

    public:
        constexpr bool await_ready() const noexcept { return false; }
        bool await_suspend(std::coroutine_handle<promise_type> h) noexcept
        {
            to_resume = self.attach(h.promise().top_waiter, this);
            return true;
        }
        constexpr void await_resume() const noexcept {}

        awaiter(std::coroutine_handle<promise_type> self)
            : self(self.promise())
        {}
    };

    task(std::coroutine_handle<promise_type> handle)
        : m_handle(handle)
    {}
    task(task&& t) noexcept
        : m_handle(std::exchange(t.m_handle, {}))
    {}
    task& operator=(task&& t) noexcept
    {
        task tmp(std::move(t));
        std::swap(m_handle, tmp.m_handle);

        return *this;
    }
    ~task()
    {
        if (m_handle)
        {
            m_handle.promise().detach();
            m_handle.destroy();
        }
    }

    bool done() const noexcept
    {
        assert(m_handle);
        return m_handle.done();
    }

    void operator()() const
    {
        assert(m_handle);
        m_handle.promise().next_step();
    }

    auto operator co_await() { return awaiter { m_handle }; }

    static std::suspend_always suspend() { return {}; }

private:
    std::coroutine_handle<promise_type> m_handle;
};

} // namespace hlasm_plugin::utils

#endif
