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
#include "fade_messages.h"
#include "location.h"
#include "semantics/highlighting_info.h"

namespace hlasm_plugin::parser_library {

//********************** location **********************

position_uri::position_uri(const location& item)
    : item_(item)
{}
position position_uri::pos() const { return item_.pos; }
std::string_view position_uri::file_uri() const { return item_.get_uri(); }

position_uri sequence_item_get(const sequence<position_uri, const location*>* self, size_t index)
{
    return position_uri(self->stor_[index]);
}

range_uri::range_uri(range_uri_s& range)
    : impl_(range)
{}

range range_uri::get_range() const { return impl_.rang; }

const char* range_uri::uri() const { return impl_.uri.c_str(); }

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
