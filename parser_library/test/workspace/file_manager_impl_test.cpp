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

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "utils/resource_location.h"
#include "workspaces/file.h"
#include "workspaces/file_manager_impl.h"

using namespace hlasm_plugin::parser_library::workspaces;
using namespace hlasm_plugin::utils::resource;
using namespace hlasm_plugin::utils;
using namespace ::testing;

namespace {
struct external_file_reader_mock : external_file_reader
{
    MOCK_METHOD(value_task<std::optional<std::string>>, load_text, (const resource_location&), (const override));
    MOCK_METHOD(value_task<list_directory_result>, list_directory_files, (const resource_location&), (const override));
};

auto load_text_coroutine(std::optional<std::string> v)
{
    return [v]() { return hlasm_plugin::utils::value_task<std::optional<std::string>>::from_value(v); };
}

} // namespace

TEST(file_manager, update_file)
{
    const resource_location file("test/library/test_wks/correct");
    const std::string text1 = "aaa";
    const std::string text2 = "bbb";

    NiceMock<external_file_reader_mock> reader_mock;
    file_manager_impl fm(reader_mock);

    // nobody is working with the file, so assume it has not changed
    EXPECT_EQ(fm.update_file(file).run().value(), open_file_result::identical);

    EXPECT_CALL(reader_mock, load_text(file)).WillOnce(Invoke(load_text_coroutine(text1)));

    auto f = fm.add_file(file).run().value();

    EXPECT_EQ(f->get_text(), text1);

    EXPECT_CALL(reader_mock, load_text(file)).WillRepeatedly(Invoke(load_text_coroutine(text2)));

    EXPECT_EQ(fm.update_file(file).run().value(), open_file_result::changed_content);
    EXPECT_EQ(f->get_text(), text1); // old file version
    EXPECT_EQ(fm.update_file(file).run().value(), open_file_result::identical);

    f = fm.add_file(file).run().value();
    EXPECT_EQ(f->get_text(), text2);

    EXPECT_EQ(fm.update_file(file).run().value(), open_file_result::identical);
}

TEST(file_manger, keep_content_on_close)
{
    const resource_location file("filename");
    const std::string text = "aaa";

    NiceMock<external_file_reader_mock> reader_mock;
    file_manager_impl fm(reader_mock);

    fm.did_open_file(file, 1, text);

    auto f1 = fm.add_file(file).run().value();

    fm.did_close_file(file);

    EXPECT_CALL(reader_mock, load_text(file)).WillOnce(Invoke(load_text_coroutine(text)));
    auto f2 = fm.add_file(file).run().value();

    EXPECT_EQ(f1.get(), f2.get());
    EXPECT_EQ(f1->get_version(), f2->get_version());
}

TEST(file_manager, get_file_content)
{
    const resource_location file("test/library/test_wks/correct");
    file_manager_impl fm;

    // nobody is working with the file, so assume it has not changed
    EXPECT_TRUE(fm.get_file_content(resource_location("test/library/test_wks/correct")).run().value().has_value());
    EXPECT_FALSE(fm.get_file_content(resource_location("test/library/test_wks/notexists")).run().value().has_value());
}
