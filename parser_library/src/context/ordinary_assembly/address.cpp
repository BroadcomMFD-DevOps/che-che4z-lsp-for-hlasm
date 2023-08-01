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

#include "address.h"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <numeric>
#include <span>
#include <type_traits>
#include <unordered_map>

#include "location_counter.h"
#include "section.h"

namespace hlasm_plugin::parser_library::context {

space::space(location_counter& owner, alignment align, space_kind kind)
    : kind(kind)
    , align(std::move(align))
    , owner(owner)
    , resolved_length(0)
    , resolved_(false)
{}

space::space(location_counter& owner, alignment align, size_t boundary, int offset)
    : kind(space_kind::LOCTR_UNKNOWN)
    , align(std::move(align))
    , previous_boundary(boundary)
    , previous_offset(offset)
    , owner(owner)
    , resolved_length(0)
    , resolved_(false)
{}

void space::resolve(space_ptr this_space, int length)
{
    if (this_space->resolved_)
        return;

    if (this_space->kind == space_kind::ALIGNMENT)
    {
        alignment& align = this_space->align;
        if (length % align.boundary != align.byte)
            length = (int)((align.boundary - (length % align.boundary)) + align.byte) % align.boundary;
        else
            length = 0;
    }

    this_space->resolved_length = length;

    this_space->owner.resolve_space(this_space, length);

    this_space->resolved_ = true;
}

void space::resolve(space_ptr this_space, space_ptr value)
{
    if (this_space->resolved_)
        return;

    assert(this_space->kind == space_kind::LOCTR_UNKNOWN);

    this_space->resolved_ptrs.push_back(address::space_entry(value, 1));

    this_space->resolved_ = true;
}

void space::resolve(space_ptr this_space, int length, std::vector<address::space_entry> unresolved)
{
    if (this_space->resolved_)
        return;

    assert(this_space->kind == space_kind::LOCTR_UNKNOWN);

    this_space->resolved_length = length;

    this_space->resolved_ptrs = std::move(unresolved);

    this_space->resolved_ = true;
}

const std::vector<address::base_entry>& address::bases() const { return bases_; }

std::vector<address::base_entry>& address::bases() { return bases_; }

int get_space_offset(const std::vector<address::space_entry>& sp_vec)
{
    return std::transform_reduce(sp_vec.begin(), sp_vec.end(), 0, std::plus<>(), [](const auto& v) {
        const auto& [sp, cnt] = v;
        return sp->resolved() ? cnt * (sp->resolved_length + get_space_offset(sp->resolved_ptrs)) : 0;
    });
}

int address::offset() const { return offset_ + (spaces_ ? get_space_offset(*spaces_) : 0); }
int address::unresolved_offset() const { return offset_; }

template<typename T, typename U>
auto&& move_if_rvalue(U&& u)
{
    if constexpr (std::is_lvalue_reference_v<T>)
        return u;
    else
        return std::move(u);
}

enum class merge_op : bool
{
    add,
    sub
};

struct normalization_helper
{
    std::unordered_map<space*, size_t> map;
};

void insert(const address::space_entry& sp,
    normalization_helper& helper,
    std::vector<address::space_entry>& normalized_spaces,
    int sp_multiplier)
{
    auto* sp_ptr = sp.first.get();
    if (auto it = helper.map.find(sp_ptr); it != helper.map.end())
    {
        normalized_spaces[it->second].second += sp_multiplier * sp.second;
    }
    else
    {
        normalized_spaces.emplace_back(sp).second *= sp_multiplier;
        helper.map.emplace(sp_ptr, normalized_spaces.size() - 1);
    }
}

int get_unresolved_spaces(const std::vector<address::space_entry>& spaces,
    normalization_helper& helper,
    std::vector<address::space_entry>& normalized_spaces,
    int multiplier)
{
    int offset = 0;
    for (auto&& sp : spaces)
    {
        if (sp.first->resolved())
        {
            offset += sp.second
                * (sp.first->resolved_length
                    + get_unresolved_spaces(
                        sp.first->resolved_ptrs, helper, normalized_spaces, multiplier * sp.second));
            // TODO: overflow check
        }
        else
            insert(sp, helper, normalized_spaces, multiplier);
    }

    return offset;
}

void cleanup_spaces(std::vector<address::space_entry>& spaces)
{
    std::erase_if(spaces, [](const address::space_entry& e) { return e.second == 0; });
}

std::pair<std::vector<address::space_entry>, int> address::normalized_spaces() const
{
    if (!spaces_)
        return {};

    std::vector<space_entry> res_spaces;
    normalization_helper helper;

    int offset = get_unresolved_spaces(*spaces_, helper, res_spaces, 1);

    cleanup_spaces(res_spaces);

    return { std::move(res_spaces), offset };
}

address::address(base address_base, int offset, const space_storage& spaces)
    : offset_(offset)
{
    bases_.emplace_back(address_base, 1);

    if (spaces.empty())
        return;

    auto new_spaces = std::make_shared<std::vector<space_entry>>();
    new_spaces->reserve(spaces.size());
    for (auto& space : spaces)
        new_spaces->emplace_back(space, 1);

    spaces_ = std::move(new_spaces);
}

address::address(base address_base, int offset, space_storage&& spaces)
    : offset_(offset)
{
    bases_.emplace_back(address_base, 1);

    if (spaces.empty())
        return;

    auto new_spaces = std::make_shared<std::vector<space_entry>>();
    new_spaces->reserve(spaces.size());
    for (auto& space : spaces)
        new_spaces->emplace_back(std::move(space), 1);

    spaces_ = std::move(new_spaces);
}

template<merge_op operation, typename C1, typename C2>
requires(std::is_same_v<std::remove_cvref_t<C1>, std::remove_cvref_t<C2>>) auto merge_entries(C1&& lhs, C2&& rhs)
{
    using T = std::remove_cvref_t<C1>::value_type;
    std::vector<T> res;
    std::vector<const T*> prhs;

    prhs.reserve(rhs.size());
    for (const auto& e : rhs)
        prhs.push_back(&e);

    for (auto&& entry : lhs)
    {
        auto it = std::find_if(prhs.begin(), prhs.end(), [&](auto e) { return e ? entry.first == e->first : false; });

        if (it != prhs.end())
        {
            int count;
            if constexpr (operation == merge_op::add)
                count = entry.second + (*it)->second; // L + R
            else
                count = entry.second - (*it)->second; // L - R

            if (count != 0)
                res.emplace_back(move_if_rvalue<C1>(entry.first), count);

            *it = nullptr;
        }
        else
        {
            res.push_back(move_if_rvalue<C1>(entry));
        }
    }

    for (auto&& rest : prhs)
    {
        if (!rest)
            continue;

        res.push_back(move_if_rvalue<C2>(*rest));
        if constexpr (operation == merge_op::sub)
            res.back().second = -res.back().second;
    }

    return res;
}

address address::operator+(const address& addr) const
{
    if (!has_spaces() && !addr.has_spaces())
        return address(merge_entries<merge_op::add>(bases_, addr.bases_), offset_ + addr.offset_, {});

    auto res_spaces = std::make_shared<std::vector<space_entry>>();
    normalization_helper helper;

    int offset = 0;
    if (spaces_)
        offset += get_unresolved_spaces(*spaces_, helper, *res_spaces, 1);
    if (addr.spaces_)
        offset += get_unresolved_spaces(*addr.spaces_, helper, *res_spaces, 1);

    cleanup_spaces(*res_spaces);

    return address(
        merge_entries<merge_op::add>(bases_, addr.bases_), offset_ + addr.offset_ + offset, std::move(res_spaces));
}

address address::operator+(int offs) const { return address(bases_, offset_ + offs, spaces_); }

address address::operator-(const address& addr) const
{
    if (!has_spaces() && !addr.has_spaces())
        return address(merge_entries<merge_op::sub>(bases_, addr.bases_), offset_ - addr.offset_, {});

    auto res_spaces = std::make_shared<std::vector<space_entry>>();
    normalization_helper helper;

    int offset = 0;
    if (spaces_)
        offset += get_unresolved_spaces(*spaces_, helper, *res_spaces, 1);
    if (addr.spaces_)
        offset -= get_unresolved_spaces(*addr.spaces_, helper, *res_spaces, -1);

    cleanup_spaces(*res_spaces);

    return address(
        merge_entries<merge_op::sub>(bases_, addr.bases_), offset_ - addr.offset_ + offset, std::move(res_spaces));
}

address address::operator-(int offs) const
{
    auto [spaces, off] = normalized_spaces();
    return address(bases_, offset_ + off - offs, std::make_shared<std::vector<space_entry>>(std::move(spaces)));
}

address address::operator-() const
{
    auto [spaces, off] = normalized_spaces();
    auto inv_bases = bases_;
    for (auto& b : inv_bases)
        b.second = -b.second;
    for (auto& s : spaces)
        s.second = -s.second;
    return address(std::move(inv_bases), -offset_ - off, std::make_shared<std::vector<space_entry>>(std::move(spaces)));
}

bool address::is_complex() const { return bases_.size() > 1; }

bool address::in_same_loctr(const address& addr) const
{
    if (!is_simple() || !addr.is_simple())
        return false;

    if (addr.bases_[0].first != bases_[0].first)
        return false;

    auto [spaces, _] = normalized_spaces();
    auto [addr_spaces, __] = addr.normalized_spaces();

    bool this_has_loctr_begin = spaces.size() && spaces[0].first->kind == space_kind::LOCTR_BEGIN;
    bool addr_has_loctr_begin = addr_spaces.size() && addr_spaces[0].first->kind == space_kind::LOCTR_BEGIN;

    if (this_has_loctr_begin && addr_has_loctr_begin)
        return spaces[0].first == addr_spaces[0].first;
    else if (!this_has_loctr_begin && !addr_has_loctr_begin)
        return true;
    else
    {
        if (spaces.size() && addr_spaces.size())
            return spaces.front().first->owner.name == addr_spaces.front().first->owner.name;
        return false;
    }
}

bool address::is_simple() const { return bases_.size() == 1 && bases_[0].second == 1; }

bool address::has_dependant_space() const
{
    if (!has_spaces() || spaces_->size() == 1 && spaces_->front().first->kind == space_kind::LOCTR_BEGIN)
        return false;
    auto [spaces, _] = normalized_spaces();
    if (spaces.empty() || spaces.size() == 1 && spaces.front().first->kind == space_kind::LOCTR_BEGIN)
        return false;
    return true;
}

bool address::has_unresolved_space() const
{
    if (!has_spaces())
        return false;

    auto [spaces, _] = normalized_spaces();

    return !spaces.empty();
}

bool address::has_spaces() const { return spaces_ && !spaces_->empty(); }

address::address(std::vector<base_entry> bases_, int offset_, std::shared_ptr<const std::vector<space_entry>> spaces_)
    : bases_(std::move(bases_))
    , offset_(offset_)
    , spaces_(std::move(spaces_))
{}

} // namespace hlasm_plugin::parser_library::context
