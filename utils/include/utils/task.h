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

#ifndef HLASMPLUGIN_UTILS_TASK_H
#define HLASMPLUGIN_UTILS_TASK_H

#include <cassert>
#include <concepts>
#include <coroutine>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

namespace hlasm_plugin::utils {

class task;
template<std::move_constructible T>
class value_task;

class task_base
{
protected:
    struct promise_type_base
    {
        std::suspend_always initial_suspend() const noexcept { return {}; }

        struct final_awaiter
        {
            constexpr bool await_ready() const noexcept { return false; }
            template<std::derived_from<promise_type_base> T>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<T> self_h) const noexcept
            {
                if (auto& self = self_h.promise(); self.to_resume)
                {
                    self.top_waiter.promise().next_step = self.to_resume;
                    self.to_resume.promise().top_waiter = self.top_waiter;
                    return self.to_resume;
                }
                else
                    return std::noop_coroutine();
            }
            constexpr void await_resume() const noexcept {}
        };
        auto final_suspend() const noexcept { return final_awaiter(); }

        void unhandled_exception()
        {
            if (!to_resume)
                throw;
            pending_exception = std::current_exception();
        }

        std::coroutine_handle<promise_type_base> handle() noexcept
        {
            return std::coroutine_handle<promise_type_base>::from_promise(*this);
        }

        std::coroutine_handle<promise_type_base> next_step = handle();
        std::coroutine_handle<promise_type_base> top_waiter = handle();
        std::coroutine_handle<promise_type_base> to_resume = {};
        std::exception_ptr pending_exception;

        template<typename T>
        T&& await_transform(T&& t) const noexcept
        {
            return static_cast<T&&>(t);
        }

        template<typename T>
        class awaiter
        {
            std::coroutine_handle<promise_type_base> task_handle;

        public:
            bool await_ready() const noexcept { return task_handle.done(); }
            template<std::derived_from<promise_type_base> U>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<U> to_resume) const noexcept
            {
                auto& task_promise = task_handle.promise();
                task_promise.top_waiter.promise().next_step = task_promise.next_step;
                task_promise.to_resume = to_resume.promise().handle();
                return task_promise.next_step;
            }
            T await_resume() const
            {
                if (auto& task_promise = task_handle.promise(); task_promise.pending_exception)
                    std::rethrow_exception(std::move(task_promise.pending_exception));

                if constexpr (!std::is_void_v<T>)
                {
                    auto& p = static_cast<typename value_task<T>::promise_type&>(task_handle.promise());
                    return std::move(p.result.value());
                }
            }

            awaiter(const awaiter&) = delete;
            awaiter(awaiter&&) = delete;
            awaiter& operator=(const awaiter&) = delete;
            awaiter& operator=(awaiter&&) = delete;

            ~awaiter() { task_handle.destroy(); }

            explicit awaiter(std::coroutine_handle<promise_type_base> task_handle)
                : task_handle(std::move(task_handle))
            {}
        };
        auto await_transform(task t) noexcept;
        template<std::move_constructible T>
        auto await_transform(value_task<T> t) noexcept;
    };

    task_base() = default;
    explicit task_base(std::coroutine_handle<promise_type_base> handle)
        : m_handle(std::move(handle))
    {}
    task_base(const task_base& t) = delete;
    task_base(task_base&& t) noexcept
        : m_handle(std::exchange(t.m_handle, {}))
    {}
    task_base& operator=(const task_base& t) = delete;
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
            m_handle.destroy();
        }
    }

    promise_type_base& promise() const
    {
        assert(m_handle);
        return m_handle.promise();
    }

public:
    bool done() const noexcept
    {
        assert(m_handle);
        return m_handle.done();
    }

    void resume() const
    {
        assert(m_handle);
        m_handle.promise().next_step.promise().top_waiter = m_handle;
        m_handle.promise().next_step();
    }

    bool valid() const noexcept { return !!m_handle; }

    void run() const
    {
        assert(m_handle);
        while (!m_handle.done())
            m_handle.promise().next_step();
    }

private:
    std::coroutine_handle<promise_type_base> m_handle;
};

class task : task_base
{
    friend struct task_base::promise_type_base;

public:
    struct promise_type : task_base::promise_type_base
    {
        task get_return_object() { return task(std::coroutine_handle<promise_type>::from_promise(*this)); }
        void return_void() const noexcept {}
        std::coroutine_handle<promise_type> typed_handle() noexcept
        {
            return std::coroutine_handle<promise_type>::from_promise(*this);
        }
    };

    task() = default;
    explicit task(std::coroutine_handle<promise_type> handle)
        : task_base(handle.promise().handle())
    {}

    static std::suspend_always suspend() { return {}; }
    static std::suspend_always yield() { return {}; }

    using task_base::done;
    using task_base::resume;
    using task_base::valid;

    task& run() &
    {
        task_base::run();
        return *this;
    }
    task run() &&
    {
        task_base::run();
        return std::move(*this);
    }
};

template<std::move_constructible T>
class value_task : task_base
{
    friend struct task_base::promise_type_base;

public:
    struct promise_type : task_base::promise_type_base
    {
        value_task get_return_object() { return value_task(std::coroutine_handle<promise_type>::from_promise(*this)); }
        void return_value(T v) noexcept(noexcept(result.emplace(std::move(v)))) { result.emplace(std::move(v)); }

        std::optional<T> result;
    };

    value_task() = default;
    explicit value_task(std::coroutine_handle<promise_type> handle)
        : task_base(handle.promise().handle())
    {}

    using task_base::done;
    using task_base::resume;
    using task_base::valid;

    T& value() const&
    {
        assert(done());

        auto& p = promise();
        if (p.pending_exception)
            std::rethrow_exception(p.pending_exception);

        return static_cast<promise_type&>(p).result.value();
    }
    T value() const&& { return std::move(std::as_const(*this).value()); }

    value_task& run() &
    {
        task_base::run();
        return *this;
    }
    value_task run() &&
    {
        task_base::run();
        return std::move(*this);
    }
};

inline auto task_base::promise_type_base::await_transform(task t) noexcept
{
    auto h = std::exchange(t.m_handle, {});

    h.promise().next_step.promise().top_waiter = top_waiter;

    return awaiter<void>(std::move(h));
}

template<std::move_constructible T>
inline auto task_base::promise_type_base::await_transform(value_task<T> t) noexcept
{
    auto h = std::exchange(t.m_handle, {});

    h.promise().next_step.promise().top_waiter = top_waiter;

    return awaiter<T>(std::move(h));
}


} // namespace hlasm_plugin::utils

#endif
