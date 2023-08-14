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

#include "common_testing.h"

#include "context/hlasm_context.h"
#include "fade_messages.h"
#include "hlasmparser_multiline.h"
#include "utils/resource_location.h"
#include "utils/task.h"
#include "workspace_manager.h"
#include "workspaces/workspace.h"

void parse_all_files(hlasm_plugin::parser_library::workspaces::workspace& ws)
{
    for (auto t = ws.parse_file(); t.valid(); t = ws.parse_file())
        t.run();
}

void run_if_valid(hlasm_plugin::utils::task t)
{
    if (t.valid())
        t.run();
}

void open_parse_and_recollect_diags(
    workspace& ws, const std::vector<hlasm_plugin::utils::resource::resource_location>& files)
{
    std::for_each(files.begin(), files.end(), [&ws](const hlasm_plugin::utils::resource::resource_location& f) {
        run_if_valid(ws.did_open_file(f));
    });
    parse_all_files(ws);

    ws.diags().clear();
    ws.collect_diags();
}

void close_parse_and_recollect_diags(
    workspace& ws, const std::vector<hlasm_plugin::utils::resource::resource_location>& files)
{
    std::for_each(files.begin(), files.end(), [&ws](const hlasm_plugin::utils::resource::resource_location& f) {
        run_if_valid(ws.did_close_file(f));
    });
    parse_all_files(ws);

    ws.diags().clear();
    ws.collect_diags();
}

bool matches_fade_messages(const std::vector<fade_message_s>& a, const std::vector<fade_message_s>& b)
{
    return std::is_permutation(a.begin(), a.end(), b.begin(), b.end(), [](const auto& msg_a, const auto& msg_b) {
        return msg_a.code == msg_b.code && msg_a.r == msg_b.r && msg_a.uri == msg_b.uri;
    });
}

bool contains_fade_messages(const std::vector<fade_message_s>& a, const std::vector<fade_message_s>& b)
{
    return std::includes(a.begin(), a.end(), b.begin(), b.end(), [](const auto& msg_a, const auto& msg_b) {
        return msg_a.code == msg_b.code && msg_a.r == msg_b.r && msg_a.uri == msg_b.uri;
    });
}

const section* get_section(hlasm_context& ctx, std::string name)
{
    auto sect = ctx.ids().find(name);
    if (!sect.has_value())
        return nullptr;

    return ctx.ord_ctx.get_section(sect.value());
}

const symbol* get_symbol(hlasm_context& ctx, std::string name)
{
    auto symbol = ctx.ids().find(name);
    if (!symbol.has_value())
        return nullptr;

    return ctx.ord_ctx.get_symbol(symbol.value());
}

std::optional<symbol_value::abs_value_t> get_symbol_abs(hlasm_context& ctx, std::string name)
{
    auto symbol = ctx.ids().find(name);
    if (!symbol.has_value())
        return std::nullopt;

    auto s = ctx.ord_ctx.get_symbol(symbol.value());
    if (!s || s->kind() != symbol_value_kind::ABS)
        return std::nullopt;

    return s->value().get_abs();
}

std::optional<symbol_value::reloc_value_t> get_symbol_reloc(hlasm_context& ctx, std::string name)
{
    auto symbol = ctx.ids().find(name);
    if (!symbol.has_value())
        return std::nullopt;

    auto s = ctx.ord_ctx.get_symbol(symbol.value());
    if (!s || s->kind() != symbol_value_kind::RELOC)
        return std::nullopt;

    return s->value().get_reloc();
}

std::optional<std::pair<int, std::string>> get_symbol_address(hlasm_context& ctx, std::string name)
{
    auto symbol = ctx.ids().find(name);
    if (!symbol.has_value())
        return std::nullopt;

    auto s = ctx.ord_ctx.get_symbol(symbol.value());
    if (!s || s->kind() != symbol_value_kind::RELOC)
        return std::nullopt;

    const auto& val = s->value().get_reloc();

    if (val.bases().size() != 1 && val.bases().front().second != 1)
        return std::nullopt;

    return std::pair<int, std::string>(val.offset(), val.bases().front().first.owner->name.to_string());
}
template<typename T>
std::optional<std::unordered_map<size_t, T>> get_var_vector_map(hlasm_context& ctx, std::string name)
{
    auto id = ctx.ids().find(name);
    if (!id.has_value())
        return std::nullopt;

    auto var = ctx.get_var_sym(id.value());
    if (!var)
        return std::nullopt;

    if (var->var_kind != context::variable_kind::SET_VAR_KIND)
        return std::nullopt;
    auto var_ = var->access_set_symbol_base();
    if (var_->type != object_traits<T>::type_enum || var_->is_scalar)
        return std::nullopt;

    auto symbol = var_->template access_set_symbol<T>();
    if (!symbol)
        return std::nullopt;

    auto keys = symbol->keys();

    std::unordered_map<size_t, T> result;
    result.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); ++i)
    {
        result.emplace(keys[i], symbol->get_value(keys[i]));
    }

    return result;
}

template std::optional<std::unordered_map<size_t, context::A_t>> get_var_vector_map(
    hlasm_context& ctx, std::string name);
template std::optional<std::unordered_map<size_t, context::B_t>> get_var_vector_map(
    hlasm_context& ctx, std::string name);
template std::optional<std::unordered_map<size_t, context::C_t>> get_var_vector_map(
    hlasm_context& ctx, std::string name);

template<typename T>
std::optional<std::vector<T>> get_var_vector(hlasm_context& ctx, std::string name)
{
    auto id = ctx.ids().find(name);
    if (!id.has_value())
        return std::nullopt;

    auto var = ctx.get_var_sym(id.value());
    if (!var)
        return std::nullopt;

    if (var->var_kind != context::variable_kind::SET_VAR_KIND)
        return std::nullopt;
    auto var_ = var->access_set_symbol_base();
    if (var_->type != object_traits<T>::type_enum || var_->is_scalar)
        return std::nullopt;

    auto symbol = var_->template access_set_symbol<T>();
    if (!symbol)
        return std::nullopt;

    auto keys = symbol->keys();

    std::vector<T> result;
    result.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); ++i)
    {
        if (i != keys[i])
            return std::nullopt;
        result.push_back(symbol->get_value(i));
    }

    return result;
}

template std::optional<std::vector<context::A_t>> get_var_vector(hlasm_context& ctx, std::string name);
template std::optional<std::vector<context::B_t>> get_var_vector(hlasm_context& ctx, std::string name);
template std::optional<std::vector<context::C_t>> get_var_vector(hlasm_context& ctx, std::string name);

template<typename T>
std::optional<T> get_var_value(hlasm_context& ctx, std::string name)
{
    auto id = ctx.ids().find(name);
    if (!id.has_value())
        return std::nullopt;

    auto var = ctx.get_var_sym(id.value());
    if (!var)
        return std::nullopt;

    if (var->var_kind != context::variable_kind::SET_VAR_KIND)
        return std::nullopt;
    auto var_ = var->access_set_symbol_base();
    if (var_->type != object_traits<T>::type_enum || !var_->is_scalar)
        return std::nullopt;

    auto symbol = var_->template access_set_symbol<T>();
    if (!symbol)
        return std::nullopt;

    return symbol->get_value();
}

template std::optional<context::A_t> get_var_value(hlasm_context& ctx, std::string name);
template std::optional<context::B_t> get_var_value(hlasm_context& ctx, std::string name);
template std::optional<context::C_t> get_var_value(hlasm_context& ctx, std::string name);
