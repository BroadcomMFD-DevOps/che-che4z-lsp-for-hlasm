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

#include "../common_testing.h"
#include "../mock_parse_lib_provider.h"
#include "context/instruction.h"

// clang-format off
std::unordered_map<std::string, const std::set<system_architecture>> instruction_compatibility_matrix = {
    { "ADDFRR", { {system_architecture::ESA }, {system_architecture::XA } } },
    { "VACD", { {system_architecture::ESA }, { system_architecture::XA }, { system_architecture::_370 } } },
    { "CLRCH", { {system_architecture::UNI }, { system_architecture::_370 } } },
    { "CLRIO", { {system_architecture::UNI }, { system_architecture::_370 }, { system_architecture::DOS } } },
    { "DFLTCC", { {system_architecture::UNI }, { system_architecture::Z15 } } },
    { "VLER", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::ESA }, { system_architecture::XA }, { system_architecture::_370 } } },
    { "AGH", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::Z14 } } },
    { "CDPT", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::Z14 }, { system_architecture::Z13 } } },
    { "VA", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::Z14 }, { system_architecture::Z13 }, { system_architecture::ESA }, { system_architecture::XA }, { system_architecture::_370 } } },
    { "BPP", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::Z14 }, { system_architecture::Z13 }, { system_architecture::Z12 } } },
    { "ADTRA", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::Z14 }, { system_architecture::Z13 }, { system_architecture::Z12 }, { system_architecture::Z11 } } },
    { "AGSI", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::Z14 }, { system_architecture::Z13 }, { system_architecture::Z12 }, { system_architecture::Z11 }, { system_architecture::Z10 } } },
    { "ADTR", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::Z14 }, { system_architecture::Z13 }, { system_architecture::Z12 }, { system_architecture::Z11 }, { system_architecture::Z10 }, { system_architecture::Z9 } } },
    { "CDSY", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::Z14 }, { system_architecture::Z13 }, { system_architecture::Z12 }, { system_architecture::Z11 }, { system_architecture::Z10 }, { system_architecture::Z9 }, { system_architecture::YOP } } },
    { "AG", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::Z14 }, { system_architecture::Z13 }, { system_architecture::Z12 }, { system_architecture::Z11 }, { system_architecture::Z10 }, { system_architecture::Z9 }, { system_architecture::YOP }, { system_architecture::ZOP } } },
    { "ADB", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::Z14 }, { system_architecture::Z13 }, { system_architecture::Z12 }, { system_architecture::Z11 }, { system_architecture::Z10 }, { system_architecture::Z9 }, { system_architecture::YOP }, { system_architecture::ZOP }, { system_architecture::ESA } } },
    { "BASSM", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::Z14 }, { system_architecture::Z13 }, { system_architecture::Z12 }, { system_architecture::Z11 }, { system_architecture::Z10 }, { system_architecture::Z9 }, { system_architecture::YOP }, { system_architecture::ZOP }, { system_architecture::ESA }, { system_architecture::XA } } },
    { "BAS", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::Z14 }, { system_architecture::Z13 }, { system_architecture::Z12 }, { system_architecture::Z11 }, { system_architecture::Z10 }, { system_architecture::Z9 }, { system_architecture::YOP }, { system_architecture::ZOP }, { system_architecture::ESA }, { system_architecture::XA }, { system_architecture::_370 } } },
    { "A", { {system_architecture::UNI }, { system_architecture::Z15 }, { system_architecture::Z14 }, { system_architecture::Z13 }, { system_architecture::Z12 }, { system_architecture::Z11 }, { system_architecture::Z10 }, { system_architecture::Z9 }, { system_architecture::YOP }, { system_architecture::ZOP }, { system_architecture::ESA }, { system_architecture::XA }, { system_architecture::_370 }, { system_architecture::DOS } } },
};
// clang-format on

namespace {
struct instruction_sets_compatibility_params
{
    system_architecture arch;

    static instruction_sets_compatibility_params set_arch(system_architecture architecture)
    {
        instruction_sets_compatibility_params params;
        params.arch = architecture;

        return params;
    }
};

class instruction_sets_fixture : public ::testing::TestWithParam<instruction_sets_compatibility_params>
{};

} // namespace

INSTANTIATE_TEST_SUITE_P(instruction_test,
    instruction_sets_fixture,
    ::testing::Values(instruction_sets_compatibility_params::set_arch(system_architecture::ZOP),
        instruction_sets_compatibility_params::set_arch(system_architecture::YOP),
        instruction_sets_compatibility_params::set_arch(system_architecture::Z9),
        instruction_sets_compatibility_params::set_arch(system_architecture::Z10),
        instruction_sets_compatibility_params::set_arch(system_architecture::Z11),
        instruction_sets_compatibility_params::set_arch(system_architecture::Z12),
        instruction_sets_compatibility_params::set_arch(system_architecture::Z13),
        instruction_sets_compatibility_params::set_arch(system_architecture::Z14),
        instruction_sets_compatibility_params::set_arch(system_architecture::Z15),
        instruction_sets_compatibility_params::set_arch(system_architecture::UNI),
        instruction_sets_compatibility_params::set_arch(system_architecture::DOS),
        instruction_sets_compatibility_params::set_arch(system_architecture::_370),
        instruction_sets_compatibility_params::set_arch(system_architecture::XA),
        instruction_sets_compatibility_params::set_arch(system_architecture::ESA)));

TEST_P(instruction_sets_fixture, instruction_set_loading)
{
    auto arch = GetParam().arch;
    std::string dummy_input;
    analyzer a(dummy_input, analyzer_options { asm_option { "", "", arch } });

    for (const auto& instr : instruction_compatibility_matrix)
    {
        auto id = a.hlasm_ctx().ids().find(instr.first);

        if (instr.second.find(arch) == instr.second.end())
        {
            EXPECT_TRUE(id == nullptr);
        }
        else
        {
            EXPECT_TRUE(id != nullptr);
        }
    }
}

namespace {
struct test_case
{
    system_architecture arch;
    int expected_var_value;
};
} // namespace

TEST(instruction_sets_fixture, identical_macro_name_inline_definition)
{
    std::string input = R"(
        MACRO
        SAM31
        GBLA &VAR
&VAR    SETA   1        
        MEND
        
        GBLA &VAR
&VAR    SETA   0    
        SAM31
)";

    test_case cases[] = { { system_architecture::_370, 1 }, { system_architecture::Z11, 1 } };

    for (const auto& c : cases)
    {
        analyzer a(input, analyzer_options { asm_option { "", "", c.arch } });
        a.analyze();
        a.collect_diags();
        EXPECT_EQ(a.diags().size(), 0);

        EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "VAR"), c.expected_var_value);
    }
}

TEST(instruction_sets_fixture, identical_macro_name_linked_definition)
{
    std::string input = R"(
        GBLA &VAR
&VAR    SETA   0    
        SAM31
)";

    std::string macro =
        R"( MACRO
        SAM31
        GBLA &VAR
&VAR    SETA   2        
        MEND
)";

    test_case cases[] = { { system_architecture::_370, 2 }, { system_architecture::Z11, 0 } };

    mock_parse_lib_provider lib_provider { { "SAM31", macro } };

    for (const auto& c : cases)
    {
        analyzer a(input, analyzer_options { asm_option { "", "", c.arch }, &lib_provider });
        a.analyze();
        a.collect_diags();
        EXPECT_EQ(a.diags().size(), 0);

        EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "VAR"), c.expected_var_value);
    }
}

TEST(instruction_sets_fixture, identical_macro_name_inline_and_linked_definition)
{
    std::string input = R"(
        MACRO
        SAM31
        GBLA &VAR
&VAR    SETA   1        
        MEND

        GBLA &VAR
&VAR    SETA   0    
        SAM31
)";

    std::string macro =
        R"( MACRO
        SAM31
        GBLA &VAR
&VAR    SETA   2        
        MEND
)";

    test_case cases[] = { { system_architecture::_370, 1 }, { system_architecture::Z11, 1 } };

    mock_parse_lib_provider lib_provider { { "SAM31", macro } };

    for (const auto& c : cases)
    {
        analyzer a(input, analyzer_options { asm_option { "", "", c.arch }, &lib_provider });
        a.analyze();
        a.collect_diags();
        EXPECT_EQ(a.diags().size(), 0);

        EXPECT_EQ(get_var_value<A_t>(a.hlasm_ctx(), "VAR"), c.expected_var_value);
    }
}
