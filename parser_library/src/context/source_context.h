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

#ifndef CONTEXT_SOURCE_CONTEXT_H
#define CONTEXT_SOURCE_CONTEXT_H

#include <memory>
#include <vector>

#include "copy_member.h"
#include "processing/processing_format.h"
#include "source_snapshot.h"

namespace hlasm_plugin::parser_library::context {

// structure holding information about currently processed source
struct source_context
{
    location current_instruction;

    // location in the file
    size_t begin_index = 0;
    size_t end_index = 0;

    // stack of copy nests
    std::vector<copy_member_invocation> copy_stack;
    // stack of processing states
    std::vector<processing::processing_kind> proc_stack;

    source_context(utils::resource::resource_location source_loc, processing::processing_kind initial);

    source_snapshot create_snapshot() const;
};

enum class file_processing_type
{
    OPENCODE,
    COPY,
    MACRO
};

struct code_scope;

struct processing_frame
{
    constexpr processing_frame(position pos, const utils::resource::resource_location* resource_loc, id_index member)
        : pos(pos)
        , resource_loc(std::move(resource_loc))
        , member_name(member)
    {}


    position pos;
    const utils::resource::resource_location* resource_loc;
    id_index member_name;

    bool operator==(const processing_frame&) const = default;
};

struct processing_frame_details
{
    processing_frame_details(position pos,
        const utils::resource::resource_location* resource_loc,
        const code_scope& scope,
        file_processing_type proc_type,
        id_index member);

    position pos;
    const utils::resource::resource_location* resource_loc;
    const code_scope& scope;
    file_processing_type proc_type;
    id_index member_name;
};

using processing_stack_details_t = std::vector<processing_frame_details>;

class processing_frame_tree
{
    struct processing_frame_node
    {
        processing_frame_node* m_parent;
        processing_frame_node* m_child;
        processing_frame_node* m_next_sibling;

        processing_frame frame;
    };

    processing_frame_node m_root = { nullptr, nullptr, nullptr, processing_frame({}, {}, {}) };

public:
    class node_pointer
    {
        processing_frame_node* m_node;

        node_pointer(processing_frame_node* node)
            : m_node(node)
        {}

    public:
        node_pointer()
            : m_node(nullptr) {};

        const processing_frame& frame() const { return m_node->frame; }
        node_pointer parent() const { return node_pointer(m_node->m_parent); }

        bool operator==(node_pointer it) const noexcept { return m_node == it.m_node; }

        bool empty() const noexcept { return !m_node || m_node->m_parent == nullptr; }

        friend class processing_frame_tree;
    };

    node_pointer root() { return node_pointer(&m_root); }

    processing_frame_tree() noexcept = default;
    processing_frame_tree(const processing_frame_tree& other);
    processing_frame_tree(processing_frame_tree&& other) noexcept
        : processing_frame_tree()
    {
        swap(other);
    }
    processing_frame_tree& operator=(const processing_frame_tree& other)
    {
        if (this == &other)
            return *this;

        processing_frame_tree tmp(other);
        swap(tmp);

        return *this;
    }
    processing_frame_tree& operator=(processing_frame_tree&& other) noexcept
    {
        processing_frame_tree tmp(std::move(other));
        swap(tmp);

        return *this;
    }
    ~processing_frame_tree();

    void swap(processing_frame_tree& other) noexcept { std::swap(m_root, other.m_root); }

    node_pointer step(processing_frame next, node_pointer current);

private:
    void recursive_dulicate(processing_frame_node* ptr, node_pointer current);
};

using processing_stack_t = processing_frame_tree::node_pointer;

} // namespace hlasm_plugin::parser_library::context
#endif
