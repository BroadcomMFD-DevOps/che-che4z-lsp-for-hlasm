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

#include "protocol.h"

#include "debugging/debug_types.h"
#include "diagnosable.h"
#include "semantics/highlighting_info.h"

namespace hlasm_plugin::parser_library {

//*********************** stack_frame *************************
stack_frame::stack_frame(const debugging::stack_frame& frame)
    : name(frame.name)
    , source_file(frame.frame_source)
    , source_range { { frame.begin_line, 0 }, { frame.end_line, 0 } }
    , id(frame.id)
{}

stack_frame sequence_item_get(const sequence<stack_frame, const debugging::stack_frame*>* self, size_t index)
{
    return stack_frame(self->stor_[index]);
}

//********************* source **********************

source::source(const debugging::source& source)
    : uri(source.uri)
{}

//*********************** scope *************************

scope::scope(const debugging::scope& impl)
    : name(impl.name)
    , variable_reference(impl.var_reference)
    , source_file(impl.scope_source)
{}

scope sequence_item_get(const sequence<scope, const debugging::scope*>* self, size_t index)
{
    return scope(self->stor_[index]);
}


//********************** variable **********************

variable::variable(const debugging::variable& impl)
    : name(impl.name)
    , value(impl.value)
    , variable_reference(impl.var_reference)
    , type(impl.type)
{}

variable sequence_item_get(const sequence<variable, const debugging::variable_store*>* self, size_t index)
{
    return variable(self->stor_->variables[index]);
}



} // namespace hlasm_plugin::parser_library
