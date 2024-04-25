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

#include "analyzer_fixture.h"
#include "completion_item.h"
#include "instruction_set_version.h"
#include "lsp/item_convertors.h"
#include "lsp/lsp_context.h"

using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::parser_library::lsp;


struct lsp_context_instr : public ::testing::Test
{
    const static inline std::string input =
        R"(
&VAR SETA 1
 ADR
)";

    const static inline auto opencode_file_loc = hlasm_plugin::utils::resource::resource_location("source");
    std::unique_ptr<analyzer> m_analyzer;

    lsp_context_instr() = default;

    std::unique_ptr<analyzer> new_analyzer(instruction_set_version instr_set = instruction_set_version::Z15)
    {
        auto a = std::make_unique<analyzer>(input,
            analyzer_options {
                opencode_file_loc, &workspaces::empty_parse_lib_provider::instance, asm_option { "", "", instr_set } });

        a->analyze();
        return a;
    }

    std::vector<completion_item> get_completion_list(instruction_set_version instr_set)
    {
        analyzer a(input,
            analyzer_options {
                opencode_file_loc, &workspaces::empty_parse_lib_provider::instance, asm_option { "", "", instr_set } });

        a.analyze();

        return generate_completion(a.context().lsp_ctx->completion(
            opencode_file_loc, { 2, 3 }, 'R', completion_trigger_kind::trigger_character));
    }
};

namespace {
auto label_addfrr_compare = [](const auto& item) { return item.label == "ADDFRR"; };
}
TEST_F(lsp_context_instr, ADDFRR_not_loaded)
{
    auto comp_list = get_completion_list(instruction_set_version::Z15);

    auto result = std::none_of(comp_list.begin(), comp_list.end(), label_addfrr_compare);

    EXPECT_TRUE(result);
}

TEST_F(lsp_context_instr, ADDFRR_loaded)
{
    auto comp_list = get_completion_list(instruction_set_version::XA);

    auto result = std::any_of(comp_list.begin(), comp_list.end(), label_addfrr_compare);

    EXPECT_TRUE(result);
}
TEST_F(lsp_context_instr, ADDFRR_loaded_changed_instr_set)
{
    auto comp_list_z15 = get_completion_list(instruction_set_version::Z15);
    auto comp_list_xa = get_completion_list(instruction_set_version::XA);

    auto result_z15 = std::none_of(comp_list_z15.begin(), comp_list_z15.end(), label_addfrr_compare);

    auto result_xa = std::any_of(comp_list_xa.begin(), comp_list_xa.end(), label_addfrr_compare);

    EXPECT_NE(comp_list_z15.size(), comp_list_xa.size());
    EXPECT_TRUE(result_z15);
    EXPECT_TRUE(result_xa);
}

TEST(lsp_completion_instr, consistency)
{
    hlasm_plugin::utils::resource::resource_location empty_loc;
    std::string input = R"(
 LA   
  LA  
   LA 
)";
    analyzer a(input);
    a.analyze();

    for (size_t l = 1; l <= 3; ++l)
    {
        for (size_t c = 0; c < 3; ++c)
        {
            EXPECT_TRUE(std::holds_alternative<completion_list_instructions>(
                a.context().lsp_ctx->completion(empty_loc, { l, l + c }, 0, completion_trigger_kind::invoked)));
        }
    }
}
