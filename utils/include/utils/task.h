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

#ifndef HLASMPLUGIN_UTILS_task_base_H
#define HLASMPLUGIN_UTILS_task_base_H

#include <cassert>
#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

namespace hlasm_plugin::utils {

class task_base
{
public:
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

    std::exception_ptr pending_exception(bool clear = false) const
    {
        assert(m_handle);
        auto& excp = m_handle.promise().next_step.promise().pending_exception;
        return clear ? std::exchange(excp, {}) : excp;
    }

protected:
    class awaiter_base;
    struct promise_type_base
    {
        std::suspend_always initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() noexcept
        {
            detach();
            return {};
        }
        void unhandled_exception()
        {
            if (!active || pending_exception)
                throw;
            pending_exception = std::current_exception();
        }

        std::coroutine_handle<promise_type_base> next_step =
            std::coroutine_handle<promise_type_base>::from_promise(*this);
        awaiter_base* active = nullptr;
        std::coroutine_handle<promise_type_base> top_waiter =
            std::coroutine_handle<promise_type_base>::from_promise(*this);
        std::exception_ptr pending_exception;

        void attach(std::coroutine_handle<promise_type_base> current_top_waiter, awaiter_base* a)
        {
            current_top_waiter.promise().next_step = std::coroutine_handle<promise_type_base>::from_promise(*this);
            active = a;
            top_waiter = std::move(current_top_waiter);
        }

        void detach() noexcept
        {
            if (active)
            {
                active->to_resume.promise().pending_exception = std::exchange(pending_exception, {});
                top_waiter.promise().next_step = active->to_resume;

                next_step = std::coroutine_handle<promise_type_base>::from_promise(*this);
                active = nullptr;
                top_waiter = std::coroutine_handle<promise_type_base>::from_promise(*this);
            }
        }
    };
    class awaiter_base
    {
    protected:
        promise_type_base& self;
        std::coroutine_handle<promise_type_base> to_resume {};

        friend struct promise_type_base;

    public:
        constexpr bool await_ready() const noexcept { return false; }
        bool await_suspend(std::coroutine_handle<promise_type_base> h) noexcept
        {
            self.attach(h.promise().top_waiter, this);
            to_resume = std::move(h);
            return true;
        }
        template<std::derived_from<promise_type_base> T>
        bool await_suspend(std::coroutine_handle<T> h) noexcept
        {
            return await_suspend(std::coroutine_handle<promise_type_base>::from_promise(h.promise()));
        }

        void await_resume() const
        {
            if (to_resume.promise().pending_exception)
                std::rethrow_exception(std::exchange(to_resume.promise().pending_exception, {}));
        }

        explicit awaiter_base(promise_type_base& self) noexcept
            : self(self)
        {}
    };

    explicit task_base(std::coroutine_handle<promise_type_base> handle)
        : m_handle(std::move(handle))
    {}
    task_base(task_base&& t) noexcept
        : m_handle(std::exchange(t.m_handle, {}))
    {}
    task_base& operator=(task_base&& t) noexcept
    {
        task_base tmp(std::move(t));
        std::swap(m_handle, tmp.m_handle);

        return *this;
    }
    ~task_base()
    {
        if (m_handle)
        {
            // pending exception will be dropped - should we do something about it?
            m_handle.promise().detach();
            m_handle.destroy();
        }
    }

    std::coroutine_handle<promise_type_base> m_handle;
};

class task : task_base
{
public:
    struct promise_type : task_base::promise_type_base
    {
        task get_return_object() { return task(std::coroutine_handle<promise_type_base>::from_promise(*this)); }
        void return_void() const noexcept {}
    };

    explicit task(std::coroutine_handle<promise_type_base> handle)
        : task_base(std::move(handle))
    {}

    auto operator co_await() const&&
    {
        class awaiter : awaiter_base
        {
        public:
            awaiter(promise_type_base& self)
                : awaiter_base(self)
            {}

            using awaiter_base::await_ready;
            using awaiter_base::await_resume;
            using awaiter_base::await_suspend;
        };
        return awaiter(m_handle.promise());
    }

    static std::suspend_always suspend() { return {}; }

    using task_base::done;
    using task_base::pending_exception;
    using task_base::operator();
};

template<std::move_constructible T>
class value_task : task_base
{
public:
    struct promise_type : task_base::promise_type_base
    {
        value_task get_return_object()
        {
            return value_task(std::coroutine_handle<promise_type_base>::from_promise(*this));
        }
        void return_value(T v) noexcept(noexcept(result.emplace(std::move(v)))) { result.emplace(std::move(v)); }

        std::optional<T> result;
    };

    explicit value_task(std::coroutine_handle<promise_type_base> handle)
        : task_base(std::move(handle))
    {}

    auto operator co_await() const&&
    {
        class awaiter : awaiter_base
        {
        public:
            awaiter(promise_type_base& self)
                : awaiter_base(self)
            {}

            using awaiter_base::await_ready;
            using awaiter_base::await_suspend;
            T await_resume() const
            {
                awaiter_base::await_resume();
                return std::move(static_cast<promise_type&>(self).result.value());
            }
        };
        return awaiter(m_handle.promise());
    }

    static std::suspend_always suspend() { return {}; }

    using task_base::done;
    using task_base::pending_exception;
    using task_base::operator();

    T& value() const&
    {
        assert(done());
        auto& p = m_handle.promise();
        if (p.pending_exception)
            std::rethrow_exception(p.pending_exception);

        return static_cast<promise_type&>(p).result.value();
    }

    T value() const&&
    {
        assert(done());
        auto& p = m_handle.promise();
        if (p.pending_exception)
            std::rethrow_exception(p.pending_exception);

        return std::move(static_cast<promise_type&>(p).result.value());
    }
};


} // namespace hlasm_plugin::utils

#endif
