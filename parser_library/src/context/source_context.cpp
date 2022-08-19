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

#include "source_context.h"

#include <cassert>

using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::parser_library::context;

source_context::source_context(utils::resource::resource_location source_loc, processing::processing_kind initial)
    : current_instruction(position(), std::move(source_loc))
    , proc_stack(1, initial)
{}

source_snapshot source_context::create_snapshot() const
{
    std::vector<copy_frame> copy_frames;

    for (auto& member : copy_stack)
        copy_frames.emplace_back(member.name(), member.current_statement);

    if (!copy_frames.empty())
        --copy_frames.back().statement_offset;

    return source_snapshot { current_instruction, begin_index, end_index, std::move(copy_frames) };
}

processing_frame_details::processing_frame_details(position pos,
    const utils::resource::resource_location* resource_loc,
    const code_scope& scope,
    file_processing_type proc_type,
    id_index member)
    : pos(pos)
    , resource_loc(std::move(resource_loc))
    , scope(scope)
    , proc_type(std::move(proc_type))
    , member_name(member)
{}

void processing_frame_tree::recursive_dulicate(processing_frame_node* ptr, node_pointer current)
{
    auto* next_node = new processing_frame_node { current.m_node, nullptr, current.m_node->m_child, ptr->frame };
    current.m_node->m_child = next_node;
    node_pointer this_copy(next_node);

    for (auto p = ptr->m_child; p; p = p->m_next_sibling)
        recursive_dulicate(p, this_copy);
}

processing_frame_tree::processing_frame_tree(const processing_frame_tree& other)
{
    for (auto p = other.m_root.m_child; p; p = p->m_next_sibling)
        recursive_dulicate(p, root());
}


processing_frame_tree::node_pointer processing_frame_tree::step(processing_frame next, node_pointer current)
{
    assert(current.m_node);

    for (auto** p = &current.m_node->m_child; *p; p = &(*p)->m_next_sibling)
    {
        if ((*p)->frame == next)
        {
            // move to front
            auto* result = *p;
            *p = result->m_next_sibling;
            result->m_next_sibling = current.m_node->m_child;
            current.m_node->m_child = result;

            return node_pointer(result);
        }
    }

    auto* next_node = new processing_frame_node { current.m_node, nullptr, current.m_node->m_child, std::move(next) };
    current.m_node->m_child = next_node;
    return node_pointer(next_node);
}


processing_frame_tree ::~processing_frame_tree()
{
    processing_frame_node* left_most = nullptr;
    const auto update_left_most = [&left_most, this]() {
        if (!left_most)
            left_most = m_root.m_child;
        while (true)
        {
            if (left_most->m_child)
                left_most = left_most->m_child;
            else if (left_most->m_next_sibling)
                left_most = left_most->m_next_sibling;
            else
                break;
        }
    };
    while (auto ch = m_root.m_child)
    {
        if (ch->m_next_sibling && ch->m_child)
        {
            update_left_most();
            left_most->m_child = std::exchange(ch->m_next_sibling, nullptr);
            continue;
        }
        if (left_most == ch)
            left_most = nullptr;
        delete std::exchange(m_root.m_child, ch->m_next_sibling ? ch->m_next_sibling : ch->m_child);
    }
}