/*
 * Copyright (c) 2024 Broadcom.
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

#include "gmock/gmock.h"

#include "lsp/folding.h"

using namespace hlasm_plugin::parser_library::lsp;
using namespace ::testing;

TEST(lsp_folding, identify_lines)
{
    const auto le = generate_indentation_map(R"(*
.* comment
    SAM31
    SAM31
L   SAM31
        SAM31
        SAM31
    SAM31
*--------- SEPARATOR ----------------------------------------
    SAM31                                                              X
    
    )");

    const line_entry expected[] {
        { 0, 1, 1, -1, true, false, true, false, false, false },
        { 1, 2, 2, -1, true, false, false, false, false, false },
        { 2, 3, 0, 4, false, false, false, false, false, false },
        { 3, 4, 0, 4, false, false, false, false, false, false },
        { 4, 5, 0, 4, false, false, false, false, true, false },
        { 5, 6, 0, 8, false, false, false, false, false, false },
        { 6, 7, 0, 8, false, false, false, false, false, false },
        { 7, 8, 0, 4, false, false, false, false, false, false },
        { 8, 9, 1, -1, true, false, false, true, false, false },
        { 9, 11, 0, 4, false, false, false, false, false, false },
        { 11, 12, 0, -1, false, true, false, false, false, false },
    };

    EXPECT_THAT(le, ElementsAreArray(expected));
}
