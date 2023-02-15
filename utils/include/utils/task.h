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
    struct promise_type
    {
        task get_return_object() { return { std::coroutine_handle<promise_type>::from_promise(*this) }; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
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
            m_handle.destroy();
    }

    bool done() const noexcept
    {
        assert(m_handle);
        return m_handle.done();
    }

    void operator()() const
    {
        assert(m_handle);
        m_handle();
    }

private:
    std::coroutine_handle<promise_type> m_handle;
};

} // namespace hlasm_plugin::utils

#endif
