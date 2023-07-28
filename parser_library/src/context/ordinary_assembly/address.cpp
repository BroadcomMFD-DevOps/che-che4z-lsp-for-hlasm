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
#include <numeric>
#include <span>
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

int address::offset() const { return offset_ + get_space_offset(spaces_); }
int address::unresolved_offset() const { return offset_; }

void insert(const address::space_entry& sp,
    std::unordered_map<space*, size_t>& normalized_map,
    std::vector<address::space_entry>& normalized_spaces)
{
    auto* sp_ptr = sp.first.get();
    if (auto it = normalized_map.find(sp_ptr); it != normalized_map.end())
        normalized_spaces[it->second].second += sp.second;
    else
    {
        normalized_spaces.push_back(sp);
        normalized_map.emplace(sp_ptr, normalized_spaces.size() - 1);
    }
}

void insert(address::space_entry&& sp,
    std::unordered_map<space*, size_t>& normalized_map,
    std::vector<address::space_entry>& normalized_spaces)
{
    auto* sp_ptr = sp.first.get();
    if (auto it = normalized_map.find(sp_ptr); it != normalized_map.end())
        normalized_spaces[it->second].second += sp.second;
    else
    {
        normalized_spaces.push_back(std::move(sp));
        normalized_map.emplace(sp_ptr, normalized_spaces.size() - 1);
    }
}

int get_unresolved_spaces(const std::vector<address::space_entry>& spaces,
    std::unordered_map<space*, size_t>& normalized_map,
    std::vector<address::space_entry>& normalized_spaces)
{
    int offset = 0;
    for (const auto& sp : spaces)
    {
        if (sp.first->resolved())
        {
            offset += sp.second
                * (sp.first->resolved_length
                    + get_unresolved_spaces(sp.first->resolved_ptrs, normalized_map, normalized_spaces));
        }
        else
            insert(sp, normalized_map, normalized_spaces);
    }

    return offset;
}

int get_unresolved_spaces(std::vector<address::space_entry>&& spaces,
    std::unordered_map<space*, size_t>& normalized_map,
    std::vector<address::space_entry>& normalized_spaces)
{
    int offset = 0;
    for (auto&& sp : spaces)
    {
        if (sp.first->resolved())
        {
            offset += sp.second
                * (sp.first->resolved_length
                    + get_unresolved_spaces(sp.first->resolved_ptrs, normalized_map, normalized_spaces));
        }
        else
            insert(std::move(sp), normalized_map, normalized_spaces);
    }

    return offset;
}

std::pair<std::vector<address::space_entry>, int> address::normalized_spaces() const&
{
    std::vector<space_entry> res_spaces;
    std::unordered_map<space*, size_t> tmp_map;

    int offset = get_unresolved_spaces(spaces_, tmp_map, res_spaces);

    std::erase_if(res_spaces, [](const space_entry& e) { return e.second == 0; });

    return { std::move(res_spaces), offset };
}

std::pair<std::vector<address::space_entry>, int> address::normalized_spaces() &&
{
    std::vector<space_entry> res_spaces;
    std::unordered_map<space*, size_t> tmp_map;

    int offset = get_unresolved_spaces(std::move(spaces_), tmp_map, res_spaces);

    std::erase_if(res_spaces, [](const space_entry& e) { return e.second == 0; });

    return { std::move(res_spaces), offset };
}

address::address(base address_base, int offset, const space_storage& spaces)
    : offset_(offset)
{
    bases_.emplace_back(address_base, 1);

    spaces_.reserve(spaces.size());
    for (auto& space : spaces)
    {
        spaces_.emplace_back(space, 1);
    }
}

address::address(base address_base, int offset, space_storage&& spaces)
    : offset_(offset)
{
    bases_.emplace_back(address_base, 1);

    spaces_.reserve(spaces.size());
    for (auto& space : spaces)
    {
        spaces_.emplace_back(std::move(space), 1);
    }
}

enum class op
{
    ADD,
    SUB
};

template<typename T, typename U>
auto&& move_if_rvalue(U&& u)
{
    if constexpr (std::is_lvalue_reference_v<T>)
        return u;
    else
        return std::move(u);
}

template<op operation, typename C1, typename C2>
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
            if constexpr (operation == op::ADD)
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
        if constexpr (operation == op::SUB)
            res.back().second = -res.back().second;
    }

    return res;
}

address address::operator+(const address& addr) const
{
    auto [left_spaces, left_offset] = normalized_spaces();
    auto [right_spaces, right_offset] = addr.normalized_spaces();
    address result(merge_entries<op::ADD>(bases_, addr.bases_),
        offset_ + left_offset + addr.offset_ + right_offset,
        merge_entries<op::ADD>(std::move(left_spaces), std::move(right_spaces)));

    if (auto known_spaces = std::partition(result.spaces_.begin(),
            result.spaces_.end(),
            [](const auto& entry) { return entry.first->kind == context::space_kind::LOCTR_UNKNOWN; });
        known_spaces != result.spaces_.begin())
        result.spaces_.erase(known_spaces, result.spaces_.end());

    return result;
}

address address::operator+(int offs) const { return address(bases_, offset_ + offs, spaces_); }

address address::operator-(const address& addr) const
{
    auto [left_spaces, left_offset] = normalized_spaces();
    auto [right_spaces, right_offset] = addr.normalized_spaces();
    address result(merge_entries<op::SUB>(bases_, addr.bases_),
        offset_ + left_offset - addr.offset_ - right_offset,
        merge_entries<op::SUB>(std::move(left_spaces), std::move(right_spaces)));

    if (auto known_spaces = std::partition(result.spaces_.begin(),
            result.spaces_.end(),
            [](const auto& entry) { return entry.first->kind == context::space_kind::LOCTR_UNKNOWN; });
        known_spaces != result.spaces_.begin())
        result.spaces_.erase(known_spaces, result.spaces_.end());

    return result;
}

address address::operator-(int offs) const
{
    auto [spaces, off] = normalized_spaces();
    return address(bases_, offset_ + off - offs, std::move(spaces));
}

address address::operator-() const
{
    auto [spaces, off] = normalized_spaces();
    auto inv_bases = bases_;
    for (auto& b : inv_bases)
        b.second = -b.second;
    for (auto& s : spaces)
        s.second = -s.second;
    return address(std::move(inv_bases), -offset_ - off, std::move(spaces));
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

bool has_unresolved_spaces(const space_ptr& sp)
{
    if (!sp->resolved())
        return true;
    for (const auto& [s, _] : sp->resolved_ptrs)
        if (has_unresolved_spaces(s))
            return true;
    return false;
}

bool address::has_dependant_space() const
{
    if (spaces_.empty() || spaces_.size() == 1 && spaces_.front().first->kind == space_kind::LOCTR_BEGIN)
        return false;
    auto [spaces, _] = normalized_spaces();
    for (size_t i = 0; i < spaces.size(); i++)
    {
        if (i == 0 && spaces[0].first->kind == space_kind::LOCTR_BEGIN)
            continue;
        if (has_unresolved_spaces(spaces[i].first))
            return true;
    }
    return false;
}

bool address::has_unresolved_space() const
{
    if (spaces_.empty())
        return false;

    auto [spaces, _] = normalized_spaces();
    for (const auto& [sp, __] : spaces)
        if (has_unresolved_spaces(sp))
            return true;
    return false;
}

address::address(std::vector<base_entry> bases_, int offset_, std::vector<space_entry> spaces_)
    : bases_(std::move(bases_))
    , offset_(offset_)
    , spaces_(std::move(spaces_))
{}

} // namespace hlasm_plugin::parser_library::context
