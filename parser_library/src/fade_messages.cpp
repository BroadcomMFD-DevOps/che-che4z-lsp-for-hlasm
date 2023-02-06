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

#include "fade_messages.h"

namespace hlasm_plugin::parser_library {

fade_message_s fade_message_s::preprocessor_statement(std::string uri, const range& range)
{
    return fade_message_s("F_P001", "Statement processed by a preprocessor", std::move(uri), range);
}

fade_message_s fade_message_s::inactive_statement(std::string uri, const range& range)
{
    return fade_message_s("F_IN001", "Inactive statement", std::move(uri), range);
}

} // namespace hlasm_plugin::parser_library
