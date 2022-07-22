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

#include "gtest/gtest.h"
#include <utils/encoding.h>

using namespace hlasm_plugin::utils::encoding;

TEST(encoding, percent_encode)
{
    EXPECT_EQ(percent_encode("abc"), "abc");
    EXPECT_EQ(percent_encode("%"), "%25");
    EXPECT_EQ(percent_encode("%25"), "%2525");
    EXPECT_EQ(percent_encode("%st"), "%25st");
}

TEST(encoding, percent_encode_and_ignore_utf8)
{
    EXPECT_EQ(percent_encode_and_ignore_utf8("abc"), "abc");
    EXPECT_EQ(percent_encode_and_ignore_utf8("%"), "%25");
    EXPECT_EQ(percent_encode_and_ignore_utf8("%25"), "%25");
    EXPECT_EQ(percent_encode_and_ignore_utf8("%st"), "%25st");
}
