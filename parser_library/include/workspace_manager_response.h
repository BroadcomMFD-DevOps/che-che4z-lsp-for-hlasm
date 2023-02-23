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

#ifndef HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_MANAGER_RESPONSE_H
#define HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_MANAGER_RESPONSE_H

#include <utility>

namespace hlasm_plugin::parser_library {

class workspace_manager_response_base
{
protected:
    struct impl_actions
    {
        void (*deleter)(void*) noexcept = nullptr;
        void (*error)(void*, int, const char*) = nullptr;
        void (*provide)(void*, void*) = nullptr;
    };
    struct invalidator_t
    {
        void* impl = nullptr;
        void (*deleter)(void*) noexcept = nullptr;
        union
        {
            void (*complex)(void*) = nullptr;
            void (*simple)();
        };
    };
    struct shared_data_base
    {
        unsigned long long counter = 1;
        invalidator_t invalidator;
        bool valid = true;

        shared_data_base() = default;
    };

    workspace_manager_response_base() = default;
    workspace_manager_response_base(void* impl, const impl_actions* actions) noexcept
        : impl(impl)
        , actions(actions)
    {}

    workspace_manager_response_base(const workspace_manager_response_base& o) noexcept
        : impl(o.impl)
        , actions(o.actions)
    {
        if (impl)
            ++static_cast<shared_data_base*>(impl)->counter;
    }
    workspace_manager_response_base(workspace_manager_response_base&& o) noexcept
        : impl(std::exchange(o.impl, nullptr))
        , actions(std::exchange(o.actions, nullptr))
    {}
    workspace_manager_response_base& operator=(const workspace_manager_response_base& o) noexcept
    {
        workspace_manager_response_base tmp(o);
        std::swap(impl, tmp.impl);
        std::swap(actions, tmp.actions);
        return *this;
    }
    workspace_manager_response_base& operator=(workspace_manager_response_base&& o) noexcept
    {
        workspace_manager_response_base tmp(std::move(o));
        std::swap(impl, tmp.impl);
        std::swap(actions, tmp.actions);
        return *this;
    }
    ~workspace_manager_response_base()
    {
        if (impl && --static_cast<shared_data_base*>(impl)->counter == 0)
        {
            change_invalidation_callback({});
            actions->deleter(impl);
        }
    }

    bool valid() const noexcept { return static_cast<shared_data_base*>(impl)->valid; }
    void error(int ec, const char* error) const { return actions->error(impl, ec, error); }
    void invalidate() const
    {
        auto* base = static_cast<shared_data_base*>(impl);
        if (!base->valid)
            return;

        base->valid = false;
        auto& i = base->invalidator;
        if (i.impl == impl)
            i.simple();
        else if (i.impl)
            i.complex(i.impl);
    }

    template<typename C>
    bool set_invalidation_callback(C t)
    {
        change_invalidation_callback(invalidator_t {
            new C(std::move(t)),
            +[](void* p) noexcept { delete static_cast<C*>(p); },
            +[](void* p) { (*static_cast<C*>(p))(); },
        });
    }

    template<typename C>
    bool set_invalidation_callback(C t) noexcept requires std::is_function_v<C>
    {
        change_invalidation_callback(invalidator_t { impl, nullptr, t });
    }

    void remove_invalidation_handler() noexcept { change_invalidation_callback({}); }

    void provide(void* t) const { actions->provide(impl, t); }

private:
    void* impl = nullptr;
    const impl_actions* actions = nullptr;

    void change_invalidation_callback(invalidator_t next_i) noexcept
    {
        auto& i = static_cast<shared_data_base*>(impl)->invalidator;

        std::swap(i, next_i);

        if (next_i.deleter)
            next_i.deleter(next_i.impl);
    }
};

template<typename T>
class workspace_manager_response : workspace_manager_response_base
{
    template<typename U>
    struct shared_data : shared_data_base
    {
        U data;

        shared_data(U data)
            : data(std::move(data))
        {}
    };
    template<typename U>
    static constexpr impl_actions get_actions = {
        .deleter = +[](void* p) noexcept { delete static_cast<shared_data<U>*>(p); },
        .error = +[](void* p, int ec, const char* error) { static_cast<shared_data<U>*>(p)->data.error(ec, error); },
        .provide =
            +[](void* p, void* t) { static_cast<shared_data<U>*>(p)->data.provide(std::move(*static_cast<T*>(t))); },
    };

public:
    workspace_manager_response() = default;
    template<typename U>
    workspace_manager_response(U u)
        : workspace_manager_response_base(new shared_data<U>(std::move(u)), &get_actions<U>)
    {}

    using workspace_manager_response_base::error;
    using workspace_manager_response_base::invalidate;
    using workspace_manager_response_base::remove_invalidation_handler;
    using workspace_manager_response_base::set_invalidation_callback;
    using workspace_manager_response_base::valid;

    void provide(T t) const { workspace_manager_response_base::provide(&t); }
};

class
{
    template<typename U, typename T>
    auto operator()(U u, void (U::*)(T)) const
    {
        return workspace_manager_response<T>(std::move(u));
    }
    template<typename U, typename T>
    auto operator()(U u, void (U::*)(T) const) const
    {
        return workspace_manager_response<T>(std::move(u));
    }

public:
    template<typename U>
    auto operator()(U u) const
    {
        return operator()(std::move(u), &U::provide);
    }

} inline constexpr make_workspace_manager_response;

} // namespace hlasm_plugin::parser_library

#endif
