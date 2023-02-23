/*
 * Copyright (c) 2019 Broadcom.
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

#ifndef HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_MANAGER_H
#define HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_MANAGER_H

// This file specifies the interface of parser library.
// The pimpl (pointer to implementation) idiom is used to hide its implementation.
// It implements LSP requests and notifications and is used by the language server.

#include <atomic>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "lib_config.h"
#include "message_consumer.h"
#include "parser_library_export.h"
#include "protocol.h"

namespace hlasm_plugin::parser_library {

namespace workspaces {
class workspace;
class parse_lib_provider;
} // namespace workspaces

using ws_id = workspaces::workspace*;

// Interface that can be implemented to be able to get list of
// diagnostics from workspace manager whenever a file is parsed
// Passes list of all diagnostics that are in all currently opened files.
class diagnostics_consumer
{
public:
    virtual void consume_diagnostics(diagnostic_list diagnostics, fade_message_list fade_messages) = 0;

protected:
    ~diagnostics_consumer() = default;
};

// Interface that can be implemented to be able to get performance metrics
//(time that parsing took, number of parsed lines, etc)
class parsing_metadata_consumer
{
public:
    virtual void consume_parsing_metadata(const parsing_metadata& metrics) = 0;

protected:
    ~parsing_metadata_consumer() = default;
};

enum class fs_change_type
{
    invalid = 0,
    created = 1,
    changed = 2,
    deleted = 3,
};

struct fs_change
{
    sequence<char> uri;
    fs_change_type change_type;
};

struct opcode_suggestion
{
    continuous_sequence<char> opcode;
    size_t distance;
};

template<typename T>
class workspace_manager_response
{
    struct impl_actions
    {
        void (*deleter)(void*) noexcept = nullptr;
        bool (*valid)(void*) noexcept = nullptr;
        void (*error)(void*, int, sequence<char>) = nullptr;
        void (*provide)(void*, T) = nullptr;
    };
    template<typename U>
    static constexpr impl_actions get_actions = {
        .deleter = +[](void* p) noexcept { delete static_cast<U*>(p); },
        .valid = +[](void* p) noexcept { return static_cast<U*>(p)->valid(); },
        .error = +[](void* p, int ec, sequence<char> error) { static_cast<U*>(p)->error(ec, error); },
        .provide = +[](void* p, T t) { static_cast<U*>(p)->provide(std::move(t)); },
    };

    void* impl = nullptr;
    const impl_actions* actions = nullptr;

    void* invalidation_impl = nullptr;
    void (*invalidation_deleter)(void*) = nullptr;
    union
    {
        void (*complex)(void*);
        void (*simple)();
    } invalidation = { .complex = nullptr };

    workspace_manager_response(void* impl, const impl_actions* actions) noexcept
        : impl(impl)
        , actions(actions)
    {}

public:
    workspace_manager_response() = default;
    template<typename U>
    workspace_manager_response(U u) noexcept
        : workspace_manager_response(new U(std::move(u), this), &get_actions<U>)
    {}
    workspace_manager_response(const workspace_manager_response&) = delete;
    workspace_manager_response(workspace_manager_response&& o) noexcept
        : impl(std::exchange(o.impl, nullptr))
        , actions(std::exchange(o.actions, nullptr))
        , invalidation_impl(std::exchange(o.invalidation_impl, nullptr))
        , invalidation_deleter(std::exchange(o.invalidation_deleter, nullptr))
        , invalidation(std::exchange(o.invalidation, {}))
    {
        if (invalidation_impl == &o)
            invalidation_impl = this;
    }
    workspace_manager_response& operator=(const workspace_manager_response&) = delete;
    workspace_manager_response& operator=(workspace_manager_response&& o) noexcept
    {
        if (this != &o)
        {
            if (impl)
                actions->deleter(impl);
            if (invalidation_deleter)
                invalidation_deleter(invalidation_impl);

            impl = std::exchange(o.impl, nullptr);
            actions = std::exchange(o.actions, nullptr);

            invalidation_impl = std::exchange(o.invalidation_impl, nullptr);
            invalidation_deleter = std::exchange(o.invalidation_deleter, nullptr);
            invalidation = std::exchange(o.invalidation, {});

            if (invalidation_impl == &o)
                invalidation_impl = this;
        }
        return *this;
    }
    ~workspace_manager_response()
    {
        if (impl)
            actions->deleter(impl);
        if (invalidation_deleter)
            invalidation_deleter(invalidation_impl);
    }

    bool valid() const { return actions->valid(impl); }
    void error(int ec, sequence<char> error) const { return actions->error(impl, ec, error); }
    void invalidate() const
    {
        if (invalidation_impl == this)
            invalidation.simple();
        else if (invalidation_impl)
            invalidation.complex(invalidation_impl);
    }

    template<typename C>
    bool set_invalidation_callback(C t)
    {
        remove_invalidation_handler();
        invalidation_impl = new C(std::move(t));
        invalidation_deleter = +[](void* p) { delete static_cast<C*>(p); };
        invalidation.complex = +[](void* p) { (*static_cast<C*>(p))(); };
    }

    template<typename C>
    bool set_invalidation_callback(C t) noexcept requires std::is_function_v<C>
    {
        remove_invalidation_handler();
        invalidation_impl = this;
        invalidation_deleter = nullptr;
        invalidation.simple = t;
    }

    void remove_invalidation_handler() noexcept
    {
        if (invalidation_deleter)
            invalidation_deleter(invalidation_impl);
        invalidation_impl = nullptr;
        invalidation_deleter = nullptr;
        invalidation.simple = nullptr;
    }

    void provide(T t) const { actions->provide(impl, std::move(t)); }
};

template<typename T, typename U>
inline auto make_workspace_manager_response(U u)
{
    return workspace_manager_response<T>(std::move(u));
}

// The main class that encapsulates all functionality of parser library.
// All the methods are C++ version of LSP and DAP methods.
class PARSER_LIBRARY_EXPORT workspace_manager
{
    class impl;

public:
    workspace_manager(std::atomic<bool>* cancel = nullptr);

    workspace_manager(const workspace_manager&) = delete;
    workspace_manager& operator=(const workspace_manager&) = delete;

    workspace_manager(workspace_manager&&) noexcept;
    workspace_manager& operator=(workspace_manager&&) noexcept;

    virtual ~workspace_manager();


    virtual size_t get_workspaces(ws_id* workspaces, size_t max_size);
    virtual size_t get_workspaces_count();

    virtual void add_workspace(const char* name, const char* uri);
    virtual ws_id find_workspace(const char* document_uri);
    virtual void remove_workspace(const char* uri);

    virtual void did_open_file(const char* document_uri, version_t version, const char* text, size_t text_size);
    virtual void did_change_file(
        const char* document_uri, version_t version, const document_change* changes, size_t ch_size);
    virtual void did_close_file(const char* document_uri);
    virtual void did_change_watched_files(sequence<fs_change> changes);

    virtual void definition(const char* document_uri, position pos, workspace_manager_response<position_uri> resp);
    virtual void references(const char* document_uri, position pos, workspace_manager_response<position_uri_list> resp);
    virtual void hover(const char* document_uri, position pos, workspace_manager_response<sequence<char>> resp);
    virtual void completion(const char* document_uri,
        position pos,
        char trigger_char,
        completion_trigger_kind trigger_kind,
        workspace_manager_response<completion_list> resp);

    virtual void semantic_tokens(
        const char* document_uri, workspace_manager_response<continuous_sequence<token_info>> resp);
    virtual void document_symbol(
        const char* document_uri, long long limit, workspace_manager_response<document_symbol_list> resp);

    virtual void configuration_changed(const lib_config& new_config, const char* whole_settings);

    // implementation of observer pattern - register consumer.
    virtual void register_diagnostics_consumer(diagnostics_consumer* consumer);
    virtual void unregister_diagnostics_consumer(diagnostics_consumer* consumer);
    virtual void register_parsing_metadata_consumer(parsing_metadata_consumer* consumer);
    virtual void unregister_parsing_metadata_consumer(parsing_metadata_consumer* consumer);
    virtual void set_message_consumer(message_consumer* consumer);

    virtual continuous_sequence<char> get_virtual_file_content(unsigned long long id) const;

    virtual continuous_sequence<opcode_suggestion> make_opcode_suggestion(
        const char* document_uri, const char* opcode, bool extended) const;

private:
    impl* impl_;
};

} // namespace hlasm_plugin::parser_library
#endif // !HLASMPLUGIN_PARSERLIBRARY_WORKSPACE_MANAGER_H
