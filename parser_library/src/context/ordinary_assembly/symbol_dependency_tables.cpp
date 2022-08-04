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

#include "symbol_dependency_tables.h"

#include <algorithm>
#include <cassert>
#include <queue>
#include <stack>
#include <stdexcept>

#include "ordinary_assembly_context.h"
#include "ordinary_assembly_dependency_solver.h"
#include "processing/instruction_sets/low_language_processor.h"
#include "processing/instruction_sets/postponed_statement_impl.h"

namespace hlasm_plugin::parser_library::context {

bool symbol_dependency_tables::check_cycle(dependant target, std::vector<dependant> dependencies)
{
    if (std::find(dependencies.begin(), dependencies.end(), target)
        != dependencies.end()) // dependencies contain target itself
    {
        resolve_dependant_default(target);
        return false;
    }

    while (!dependencies.empty())
    {
        auto top_dep = std::move(dependencies.back());
        dependencies.pop_back();

        auto dependant = find_dependant(top_dep);
        if (!dependant)
            continue;

        for (auto&& dep : extract_dependencies(dependant->m_resolvable, dependant->m_dec))
        {
            if (dep == target)
            {
                resolve_dependant_default(target);
                return false;
            }
            dependencies.push_back(std::move(dep));
        }
    }
    return true;
}

struct resolve_dependant_visitor
{
    symbol_value& val;
    loctr_dependency_resolver* resolver;
    ordinary_assembly_context& sym_ctx_;
    std::unordered_map<dependant, statement_ref>& dependency_source_stmts_;
    const dependency_evaluation_context& dep_ctx;

    void operator()(const attr_ref& ref) const
    {
        assert(ref.attribute == data_attr_kind::L || ref.attribute == data_attr_kind::S);

        auto tmp_sym = sym_ctx_.get_symbol(ref.symbol_id);
        assert(!tmp_sym->attributes().is_defined(ref.attribute));

        symbol_attributes::value_t value = (val.value_kind() == symbol_value_kind::ABS)
            ? val.get_abs()
            : symbol_attributes::default_value(ref.attribute);

        if (ref.attribute == data_attr_kind::L)
            tmp_sym->set_length(value);
        if (ref.attribute == data_attr_kind::S)
            tmp_sym->set_scale(value);
    }
    void operator()(id_index symbol) const { sym_ctx_.get_symbol(symbol)->set_value(val); }
    void operator()(const space_ptr& sp) const
    {
        int length = 0;
        if (sp->kind == space_kind::ORDINARY || sp->kind == space_kind::LOCTR_MAX || sp->kind == space_kind::LOCTR_SET)
            length = (val.value_kind() == symbol_value_kind::ABS && val.get_abs() >= 0) ? val.get_abs() : 0;
        else if (sp->kind == space_kind::ALIGNMENT)
            length = val.value_kind() == symbol_value_kind::RELOC ? val.get_reloc().offset() : 0;

        if (sp->kind == space_kind::LOCTR_UNKNOWN)
            resolver->resolve_unknown_loctr_dependency(sp,
                val.get_reloc(),
                dependency_source_stmts_.find(sp)->second.stmt_ref->first->resolved_stmt()->stmt_range_ref(),
                dep_ctx);
        else
            space::resolve(sp, length);
    }
};

void symbol_dependency_tables::resolve_dependant(dependant target,
    const resolvable* dep_src,
    loctr_dependency_resolver* resolver,
    const dependency_evaluation_context& dep_ctx)
{
    context::ordinary_assembly_dependency_solver dep_solver(sym_ctx_, dep_ctx);
    symbol_value val = dep_src->resolve(dep_solver);

    std::visit(resolve_dependant_visitor { val, resolver, sym_ctx_, dependency_source_stmts_, dep_ctx }, target);
}

struct resolve_dependant_default_visitor
{
    ordinary_assembly_context& sym_ctx_;

    void operator()(const attr_ref& ref) const
    {
        assert(ref.attribute == data_attr_kind::L || ref.attribute == data_attr_kind::S);
        auto tmp_sym = sym_ctx_.get_symbol(ref.symbol_id);
        assert(!tmp_sym->attributes().is_defined(ref.attribute));
        if (ref.attribute == data_attr_kind::L)
            tmp_sym->set_length(1);
        if (ref.attribute == data_attr_kind::S)
            tmp_sym->set_scale(0);
    }
    void operator()(id_index symbol) const
    {
        auto tmp_sym = sym_ctx_.get_symbol(symbol);
        tmp_sym->set_value(0);
    }
    void operator()(const space_ptr& sp) const { space::resolve(sp, 1); }
};

void symbol_dependency_tables::resolve_dependant_default(const dependant& target)
{
    std::visit(resolve_dependant_default_visitor { sym_ctx_ }, target);
}

constexpr const auto convert_to_dependant = []<typename T>(T&& x) -> dependant { return std::forward<T>(x); };
constexpr const auto convert_to_symbolic = []<typename T>(T&& x) -> std::variant<id_index, attr_ref> {
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, id_index> || std::is_same_v<std::remove_cvref_t<T>, attr_ref>)
        return std::forward<T>(x);
    else
    {
        assert(false);
        return {};
    }
};

void symbol_dependency_tables::resolve(loctr_dependency_resolver* resolver)
{
    const auto resolvable = [this]<typename T>(const std::pair<T, dependency_value>& v) {
        return extract_dependencies(v.second.m_resolvable, v.second.m_dec).empty();
    };

    while (true)
    {
        if (auto to_resolve = std::find_if(m_dependencies_symbolic.begin(), m_dependencies_symbolic.end(), resolvable);
            to_resolve != m_dependencies_symbolic.end())
        {
            auto r = std::visit(convert_to_dependant, to_resolve->first);
            const auto& [dep_src, context] = to_resolve->second;

            resolve_dependant(r, dep_src, resolver, context); // resolve target
            try_erase_source_statement(r);
            m_dependencies_symbolic.erase(to_resolve);
            continue;
        }

        if (resolver)
        {
            if (auto to_resolve = std::find_if(m_dependencies_space.begin(), m_dependencies_space.end(), resolvable);
                to_resolve != m_dependencies_space.end())
            {
                const auto& [dep_src, context] = to_resolve->second;

                resolve_dependant(to_resolve->first, dep_src, resolver, context); // resolve target
                try_erase_source_statement(to_resolve->first);
                m_dependencies_space.erase(to_resolve);
                continue;
            }
        }

        break;
    }
}

const symbol_dependency_tables::dependency_value* symbol_dependency_tables::find_dependant(
    const dependant& target) const
{
    return std::visit(
        [this]<typename T>(const T& v) -> const dependency_value* {
            if constexpr (std::is_same_v<T, space_ptr>)
            {
                if (auto it = m_dependencies_space.find(v); it == m_dependencies_space.end())
                    return nullptr;
                else
                    return &it->second;
            }
            else
            {
                if (auto it = m_dependencies_symbolic.find(v); it == m_dependencies_symbolic.end())
                    return nullptr;
                else
                    return &it->second;
            }
        },
        target);
}

void symbol_dependency_tables::erase_dependant(const dependant& target)
{
    if (std::holds_alternative<space_ptr>(target))
        m_dependencies_space.erase(std::get<space_ptr>(target));
    else
        m_dependencies_symbolic.erase(std::visit(convert_to_symbolic, target));
}

std::vector<dependant> symbol_dependency_tables::extract_dependencies(
    const resolvable* dependency_source, const dependency_evaluation_context& dep_ctx)
{
    std::vector<dependant> ret;
    context::ordinary_assembly_dependency_solver dep_solver(sym_ctx_, dep_ctx);
    auto deps = dependency_source->get_dependencies(dep_solver);

    ret.insert(ret.end(),
        std::make_move_iterator(deps.undefined_symbols.begin()),
        std::make_move_iterator(deps.undefined_symbols.end()));

    if (!ret.empty())
        return ret;

    ret.insert(ret.end(),
        std::make_move_iterator(deps.unresolved_spaces.begin()),
        std::make_move_iterator(deps.unresolved_spaces.end()));

    if (!ret.empty())
        return ret;

    ret.insert(ret.end(),
        std::make_move_iterator(deps.undefined_attr_refs.begin()),
        std::make_move_iterator(deps.undefined_attr_refs.end()));

    if (deps.unresolved_address)
        for (auto& [space_id, count] : deps.unresolved_address->normalized_spaces())
        {
            assert(count != 0);
            ret.push_back(space_id);
        }


    return ret;
}

std::vector<dependant> symbol_dependency_tables::extract_dependencies(
    const std::vector<const resolvable*>& dependency_sources, const dependency_evaluation_context& dep_ctx)
{
    std::vector<dependant> ret;

    for (auto dep : dependency_sources)
    {
        auto tmp = extract_dependencies(dep, dep_ctx);
        ret.insert(ret.end(), std::make_move_iterator(tmp.begin()), std::make_move_iterator(tmp.end()));
    }
    return ret;
}

void symbol_dependency_tables::try_erase_source_statement(const dependant& index)
{
    auto ait = dependency_source_addrs_.find(index);
    if (ait != dependency_source_addrs_.end())
        dependency_source_addrs_.erase(ait);

    auto it = dependency_source_stmts_.find(index);
    if (it == dependency_source_stmts_.end())
        return;

    auto& [dep, ref] = *it;

    assert(ref.ref_count >= 1);

    if (--ref.ref_count == 0)
        postponed_stmts_.erase(ref.stmt_ref);

    dependency_source_stmts_.erase(it);
}

symbol_dependency_tables::symbol_dependency_tables(ordinary_assembly_context& sym_ctx)
    : sym_ctx_(sym_ctx)
{}

bool symbol_dependency_tables::add_dependency(dependant target,
    const resolvable* dependency_source,
    bool check_for_cycle,
    const dependency_evaluation_context& dep_ctx)
{
    if (find_dependant(target))
        throw std::invalid_argument("symbol dependency already present");


    if (check_for_cycle)
    {
        auto dependencies = extract_dependencies(dependency_source, dep_ctx);

        bool no_cycle = check_cycle(target, dependencies);
        if (!no_cycle)
        {
            resolve(nullptr);
            return false;
        }
    }

    if (std::holds_alternative<space_ptr>(target))
        m_dependencies_space.try_emplace(std::get<space_ptr>(std::move(target)), dependency_source, dep_ctx);
    else
        m_dependencies_symbolic.try_emplace(
            std::visit(convert_to_symbolic, std::move(target)), dependency_source, dep_ctx);

    return true;
}

bool symbol_dependency_tables::add_dependency(id_index target,
    const resolvable* dependency_source,
    post_stmt_ptr dependency_source_stmt,
    const dependency_evaluation_context& dep_ctx)
{
    dependency_adder adder(*this, std::move(dependency_source_stmt), dep_ctx);
    bool added = adder.add_dependency(target, dependency_source);
    adder.finish();

    return added;
}

bool symbol_dependency_tables::add_dependency(id_index target,
    data_attr_kind attr,
    const resolvable* dependency_source,
    post_stmt_ptr dependency_source_stmt,
    const dependency_evaluation_context& dep_ctx)
{
    dependency_adder adder(*this, std::move(dependency_source_stmt), dep_ctx);
    bool added = adder.add_dependency(target, attr, dependency_source);
    adder.finish();

    return added;
}

void symbol_dependency_tables::add_dependency(space_ptr space,
    const resolvable* dependency_source,
    post_stmt_ptr dependency_source_stmt,
    const dependency_evaluation_context& dep_ctx)
{
    dependency_adder adder(*this, std::move(dependency_source_stmt), dep_ctx);
    adder.add_dependency(space, dependency_source);
    adder.finish();
}

void symbol_dependency_tables::add_dependency(space_ptr target,
    addr_res_ptr dependency_source,
    const dependency_evaluation_context& dep_ctx,
    post_stmt_ptr dependency_source_stmt)
{
    auto [it, inserted] = dependency_source_addrs_.emplace(target, std::move(dependency_source));

    add_dependency(dependant(target), &*it->second, false, dep_ctx);

    if (dependency_source_stmt)
    {
        auto [sit, sinserted] = postponed_stmts_.try_emplace(std::move(dependency_source_stmt), dep_ctx);

        if (!sinserted)
            throw std::runtime_error("statement already registered");

        dependency_source_stmts_.emplace(dependant(std::move(target)), statement_ref(sit, 1));
    }
}

bool symbol_dependency_tables::check_cycle(space_ptr target)
{
    auto dep_src = find_dependant(target);
    if (!dep_src)
        return true;

    bool no_cycle = check_cycle(target, extract_dependencies(dep_src->m_resolvable, dep_src->m_dec));

    if (!no_cycle)
        resolve(nullptr);

    return no_cycle;
}

void symbol_dependency_tables::add_dependency(post_stmt_ptr target, const dependency_evaluation_context& dep_ctx)
{
    dependency_adder adder(*this, std::move(target), dep_ctx);
    adder.add_dependency();
    adder.finish();
}

dependency_adder symbol_dependency_tables::add_dependencies(
    post_stmt_ptr dependency_source_stmt, const dependency_evaluation_context& dep_ctx)
{
    return dependency_adder(*this, std::move(dependency_source_stmt), dep_ctx);
}

void symbol_dependency_tables::add_defined(loctr_dependency_resolver* resolver) { resolve(resolver); }

bool symbol_dependency_tables::check_loctr_cycle()
{
    std::unordered_map<dependant, std::vector<dependant>> dep_g;

    // create graph
    for (const auto& [target, dep_src_loctr] : m_dependencies_space)
    {
        auto new_deps = extract_dependencies(dep_src_loctr.m_resolvable, dep_src_loctr.m_dec);

        std::erase_if(new_deps, [](const auto& e) { return !std::holds_alternative<space_ptr>(e); });

        if (!new_deps.empty())
            dep_g.emplace(target, std::move(new_deps));
    }

    std::unordered_set<dependant> visited;
    std::unordered_set<dependant> cycles;
    std::vector<dependant> path;
    std::deque<std::optional<dependant>> next_steps;

    for (const auto& [target, _] : dep_g)
    {
        if (visited.contains(target))
            continue;

        next_steps.push_back(target);

        while (!next_steps.empty())
        {
            auto next = std::move(next_steps.front());
            next_steps.pop_front();
            if (!next.has_value())
            {
                path.pop_back();
                continue;
            }
            if (auto it = std::find(path.begin(), path.end(), next.value()); it != path.end())
            {
                cycles.insert(it, path.end());
                continue;
            }
            if (!visited.emplace(next.value()).second)
                continue;
            if (auto it = dep_g.find(next.value()); it != dep_g.end())
            {
                path.push_back(std::move(next).value());
                next_steps.emplace_front(std::nullopt);
                next_steps.insert(next_steps.begin(), it->second.begin(), it->second.end());
            }
        }
        assert(path.empty());
    }

    for (const auto& target : cycles)
    {
        resolve_dependant_default(target);
        try_erase_source_statement(target);
        erase_dependant(target);
    }

    return cycles.empty();
}

std::vector<std::pair<post_stmt_ptr, dependency_evaluation_context>> symbol_dependency_tables::collect_postponed()
{
    std::vector<std::pair<post_stmt_ptr, dependency_evaluation_context>> res;

    res.reserve(postponed_stmts_.size());
    for (auto it = postponed_stmts_.begin(); it != postponed_stmts_.end();)
    {
        auto node = postponed_stmts_.extract(it++);
        res.emplace_back(std::move(node.key()), std::move(node.mapped()));
    }

    postponed_stmts_.clear();
    dependency_source_stmts_.clear();
    m_dependencies_symbolic.clear();
    m_dependencies_space.clear();

    return res;
}

void symbol_dependency_tables::resolve_all_as_default()
{
    for (auto& [target, dep_src] : m_dependencies_symbolic)
        resolve_dependant_default(std::visit(convert_to_dependant, target));
    for (auto& [target, dep_src] : m_dependencies_space)
        resolve_dependant_default(target);
}

statement_ref::statement_ref(ref_t stmt_ref, size_t ref_count)
    : stmt_ref(std::move(stmt_ref))
    , ref_count(ref_count)
{}

dependency_adder::dependency_adder(
    symbol_dependency_tables& owner, post_stmt_ptr dependency_source_stmt, const dependency_evaluation_context& dep_ctx)
    : owner_(owner)
    , ref_count_(0)
    , dep_ctx(dep_ctx)
    , source_stmt(std::move(dependency_source_stmt))
{}

bool dependency_adder::add_dependency(id_index target, const resolvable* dependency_source)
{
    bool added = owner_.add_dependency(dependant(target), dependency_source, true, dep_ctx);

    if (added)
    {
        ++ref_count_;
        dependants.push_back(target);
    }

    return added;
}

bool dependency_adder::add_dependency(id_index target, data_attr_kind attr, const resolvable* dependency_source)
{
    bool added = owner_.add_dependency(dependant(attr_ref { attr, target }), dependency_source, true, dep_ctx);

    if (added)
    {
        ++ref_count_;
        dependants.push_back(attr_ref { attr, target });
    }

    return added;
}

void dependency_adder::add_dependency(space_ptr target, const resolvable* dependency_source)
{
    owner_.add_dependency(dependant(target), dependency_source, false, dep_ctx);
    ++ref_count_;
    dependants.push_back(target);
}

void dependency_adder::add_dependency() { ++ref_count_; }

void dependency_adder::finish()
{
    if (ref_count_ == 0)
        return;

    auto [it, inserted] = owner_.postponed_stmts_.try_emplace(std::move(source_stmt), dep_ctx);

    if (!inserted)
        throw std::runtime_error("statement already registered");

    statement_ref ref(it, ref_count_);

    for (auto& dep : dependants)
        owner_.dependency_source_stmts_.emplace(std::move(dep), ref);

    dependants.clear();
}

} // namespace hlasm_plugin::parser_library::context
