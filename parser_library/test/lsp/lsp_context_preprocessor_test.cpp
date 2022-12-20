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

#include <algorithm>
#include <optional>
#include <string>

#include "gtest/gtest.h"

#include "../mock_parse_lib_provider.h"
#include "analyzer.h"
#include "context/instruction.h"
#include "location.h"
#include "range.h"
#include "utils/resource_location.h"
#include "workspaces/parse_lib_provider.h"

using namespace hlasm_plugin::parser_library;
using namespace hlasm_plugin::utils::resource;
using namespace hlasm_plugin::parser_library::lsp;

namespace {
const resource_location mac_loc("MAC");
const resource_location source_loc("OPEN");
const resource_location member_loc("MEMBER");
const resource_location member2_loc("MEMBER2");

const std::vector<std::pair<std::string, std::string>> member_list {
    { "MEMBER", R"(R2 EQU 2
            LR R2,R2)" },
    { "MEMBER2", R"(R5 EQU 5
            LR R5,R5)" },
};

class lsp_context_preprocessor_test : public testing::Test
{
public:
    lsp_context_preprocessor_test(const std::string& contents,
        std::shared_ptr<workspaces::parse_lib_provider> lib_provider,
        preprocessor_options preproc_options)
        : lib_provider(lib_provider)
        , a(contents, analyzer_options { source_loc, lib_provider.get(), preproc_options })
    {}

    void SetUp() override { a.analyze(); }

protected:
    std::shared_ptr<workspaces::parse_lib_provider> lib_provider;
    analyzer a;

    std::optional<resource_location> find_preproc_file(std::string_view name)
    {
        const auto& files = a.hlasm_ctx().get_visited_files();

        auto it = std::find_if(
            files.begin(), files.end(), [name](const resource_location& f) { return f.get_uri().ends_with(name); });

        return it != files.end() ? std::make_optional(*it) : std::nullopt;
    }
};
} // namespace

class lsp_context_endevor_preprocessor_test : public lsp_context_preprocessor_test
{
private:
    inline static const std::string contents =
        R"(
-INC  MEMBER blabla
++INCLUDE  MEMBER blabla
-INC  MEMBER2)";

public:
    lsp_context_endevor_preprocessor_test()
        : lsp_context_preprocessor_test(
            contents, std::make_shared<mock_parse_lib_provider>(member_list), endevor_preprocessor_options())
    {}
};

namespace {
static bool has_same_content(const location_list& a, const location_list& b)
{
    return std::is_permutation(a.begin(), a.end(), b.begin(), b.end());
}
} // namespace

TEST_F(lsp_context_endevor_preprocessor_test, go_to)
{
    // no jump, instr -INC
    EXPECT_EQ(location(position(1, 1), source_loc), a.context().lsp_ctx->definition(source_loc, position(1, 1)));
    // no jump, instr ++INCLUDE
    EXPECT_EQ(location(position(2, 5), source_loc), a.context().lsp_ctx->definition(source_loc, position(2, 5)));
    // no jump, instr -INC
    EXPECT_EQ(location(position(3, 1), source_loc), a.context().lsp_ctx->definition(source_loc, position(3, 1)));

    // jump from source to included file
    EXPECT_EQ(location(position(0, 0), member_loc), a.context().lsp_ctx->definition(source_loc, position(1, 8)));
    // jump from source to included file
    EXPECT_EQ(location(position(0, 0), member_loc), a.context().lsp_ctx->definition(source_loc, position(2, 14)));
    // jump from source to included file
    EXPECT_EQ(location(position(0, 0), member2_loc), a.context().lsp_ctx->definition(source_loc, position(3, 8)));

    // no jump
    EXPECT_EQ(location(position(1, 15), source_loc), a.context().lsp_ctx->definition(source_loc, position(1, 15)));
    // no jump
    EXPECT_EQ(location(position(2, 21), source_loc), a.context().lsp_ctx->definition(source_loc, position(2, 21)));
    // no jump
    EXPECT_EQ(location(position(3, 15), source_loc), a.context().lsp_ctx->definition(source_loc, position(3, 15)));
}

TEST_F(lsp_context_endevor_preprocessor_test, refs)
{
    const location_list expected_inc_locations {
        location(position(1, 0), source_loc),
        location(position(2, 0), source_loc),
        location(position(3, 0), source_loc),
    };
    const location_list expected_member_locations {
        location(position(1, 6), source_loc),
        location(position(2, 11), source_loc),
    };
    const location_list expected_member2_locations {
        location(position(3, 6), source_loc),
    };
    const location_list expected_blabla_locations {};

    // -INC/++INCLUDE reference
    EXPECT_TRUE(has_same_content(expected_inc_locations, a.context().lsp_ctx->references(source_loc, position(1, 1))));
    // -INC/++INCLUDE reference
    EXPECT_TRUE(has_same_content(expected_inc_locations, a.context().lsp_ctx->references(source_loc, position(2, 5))));
    // -INC/++INCLUDE reference
    EXPECT_TRUE(has_same_content(expected_inc_locations, a.context().lsp_ctx->references(source_loc, position(3, 2))));

    // MEMBER reference
    EXPECT_TRUE(
        has_same_content(expected_member_locations, a.context().lsp_ctx->references(source_loc, position(1, 8))));
    // MEMBER reference
    EXPECT_TRUE(
        has_same_content(expected_member_locations, a.context().lsp_ctx->references(source_loc, position(2, 14))));
    // MEMBER reference
    EXPECT_TRUE(
        has_same_content(expected_member2_locations, a.context().lsp_ctx->references(source_loc, position(3, 8))));

    // blabla reference
    EXPECT_TRUE(
        has_same_content(expected_blabla_locations, a.context().lsp_ctx->references(source_loc, position(1, 15))));
    // blabla reference
    EXPECT_TRUE(
        has_same_content(expected_blabla_locations, a.context().lsp_ctx->references(source_loc, position(2, 21))));
}

class lsp_context_cics_preprocessor_test : public lsp_context_preprocessor_test
{
private:
    inline static const std::string contents =
        R"(
A   EXEC CICS ABEND ABCODE('1234') NODUMP
  EXEC  CICS  ALLOCATE SYSID('4321') NOQUEUE
     EXEC  CICS  ABEND  ABCODE('12                                     x12345678
                 34') NODUMP

B   LARL 0,DFHRESP(NORMAL)
    L   0,DFHVALUE ( BUSY )
    L     0,DFHRESP ( NORMAL )

    LARL 1,A
    LARL 1,B)";

protected:
    std::optional<resource_location> preproc1_loc;
    std::optional<resource_location> preproc6_loc;

public:
    lsp_context_cics_preprocessor_test()
        : lsp_context_preprocessor_test(
            contents, std::make_shared<workspaces::empty_parse_lib_provider>(), cics_preprocessor_options())
    {}

    void SetUp() override
    {
        lsp_context_preprocessor_test::SetUp();

        preproc1_loc = find_preproc_file("PREPROCESSOR_1.hlasm");
        preproc6_loc = find_preproc_file("PREPROCESSOR_6.hlasm");

        ASSERT_TRUE(preproc1_loc.has_value());
        ASSERT_TRUE(preproc6_loc.has_value());
    }
};

TEST_F(lsp_context_cics_preprocessor_test, go_to)
{
    // Todo make the 2 step definition() work
    //// Jump to label in main document, label A
    // EXPECT_EQ(location(position(1, 0), source_loc), a.context().lsp_ctx->definition(source_loc, position(10, 12)));
    //// Jump to label in virtual file, label A
    // EXPECT_EQ(location(position(1, 0), *preproc1_loc), a.context().lsp_ctx->definition(source_loc, position(1, 0)));

    // no jump, EXEC CICS ABEND
    EXPECT_EQ(location(position(1, 16), source_loc), a.context().lsp_ctx->definition(source_loc, position(1, 16)));
    // no jump, operand ABCODE('1234')
    EXPECT_EQ(location(position(1, 23), source_loc), a.context().lsp_ctx->definition(source_loc, position(1, 23)));
    // no jump, operand NODUMP
    EXPECT_EQ(location(position(1, 41), source_loc), a.context().lsp_ctx->definition(source_loc, position(1, 41)));

    // Todo make the 2 step definition() work
    //// Jump to label in main document, label B
    // EXPECT_EQ(location(position(6, 0), source_loc), a.context().lsp_ctx->definition(source_loc, position(11, 12)));
    //  Jump to label in virtual file, label B
    EXPECT_EQ(location(position(1, 0), *preproc6_loc), a.context().lsp_ctx->definition(source_loc, position(6, 1)));

    // no jump, instr LARL
    EXPECT_EQ(location(position(6, 7), source_loc), a.context().lsp_ctx->definition(source_loc, position(6, 7)));
    // no jump, operand 0
    EXPECT_EQ(location(position(6, 11), source_loc), a.context().lsp_ctx->definition(source_loc, position(6, 11)));
    // no jump, operand DFHRESP(NORMAL)
    EXPECT_EQ(location(position(6, 17), source_loc), a.context().lsp_ctx->definition(source_loc, position(6, 17)));
}

TEST_F(lsp_context_cics_preprocessor_test, refs_exec_cics)
{
    const location_list expected_exec_cics_abend_locations {
        location(position(1, 4), source_loc),
        location(position(3, 5), source_loc),
    };
    const location_list expected_abcode1234_locations {
        location(position(1, 20), source_loc),
        location(position(3, 24), source_loc),
    };
    const location_list expected_nodump_locations {
        location(position(1, 35), source_loc),
        location(position(4, 22), source_loc),
    };
    const location_list expected_exec_cics_allocate_locations {
        location(position(2, 2), source_loc),
    };
    const location_list expected_sysid4321_locations {
        location(position(2, 23), source_loc),
    };
    const location_list expected_noqueue_locations {
        location(position(2, 37), source_loc),
    };

    // EXEC CICS ABEND reference
    EXPECT_TRUE(has_same_content(
        expected_exec_cics_abend_locations, a.context().lsp_ctx->references(source_loc, position(1, 7))));
    // ABCODE('1234') reference
    EXPECT_TRUE(
        has_same_content(expected_abcode1234_locations, a.context().lsp_ctx->references(source_loc, position(1, 25))));
    // NODUMP reference
    EXPECT_TRUE(
        has_same_content(expected_nodump_locations, a.context().lsp_ctx->references(source_loc, position(1, 39))));

    // ABCODE('1234') reference - multiline
    EXPECT_TRUE(
        has_same_content(expected_abcode1234_locations, a.context().lsp_ctx->references(source_loc, position(4, 18))));
    // NODUMP reference
    EXPECT_TRUE(
        has_same_content(expected_nodump_locations, a.context().lsp_ctx->references(source_loc, position(4, 25))));

    // ALLOCATE reference
    EXPECT_TRUE(has_same_content(
        expected_exec_cics_allocate_locations, a.context().lsp_ctx->references(source_loc, position(2, 18))));
    // SYSID('4321') reference
    EXPECT_TRUE(
        has_same_content(expected_sysid4321_locations, a.context().lsp_ctx->references(source_loc, position(2, 25))));
    // NOQUEUE reference
    EXPECT_TRUE(
        has_same_content(expected_noqueue_locations, a.context().lsp_ctx->references(source_loc, position(2, 42))));
}

TEST_F(lsp_context_cics_preprocessor_test, refs_dfh)
{
    const location_list expected_larl_locations {
        location(position(6, 4), source_loc),
        location(position(1, 9), *preproc6_loc),
        location(position(10, 4), source_loc),
        location(position(11, 4), source_loc),
    };
    const location_list expected_l_locations {
        location(position(7, 4), source_loc),
        location(position(8, 4), source_loc),
        location(position(3, 9), *preproc6_loc),
        location(position(5, 9), *preproc6_loc),
    };
    const location_list expected_dfhresp_normal_locations {
        location(position(6, 11), source_loc),
        location(position(8, 12), source_loc),
    };
    const location_list expected_dfhvalue_busy_locations {
        location(position(7, 10), source_loc),
    };

    // LARL reference
    EXPECT_TRUE(has_same_content(expected_larl_locations, a.context().lsp_ctx->references(source_loc, position(6, 7))));
    // L reference
    EXPECT_TRUE(has_same_content(expected_l_locations, a.context().lsp_ctx->references(source_loc, position(7, 5))));
    // DFHRESP(NORMAL) reference
    EXPECT_TRUE(has_same_content(
        expected_dfhresp_normal_locations, a.context().lsp_ctx->references(source_loc, position(6, 16))));
    // DFHVALUE ( BUSY ) reference
    EXPECT_TRUE(has_same_content(
        expected_dfhvalue_busy_locations, a.context().lsp_ctx->references(source_loc, position(7, 25))));
}

// TODO: hover for DFHVALUE and DHFRESP values

class lsp_context_db2_preprocessor_test : public lsp_context_preprocessor_test
{
protected:
    inline static const std::string contents =
        R"(
A      EXEC   SQL  INCLUDE  MEMBER
B       EXEC  SQL   INCLUDE sqlca
C     EXEC SQL INCLUDE  SqLdA)";

    std::optional<resource_location> preproc2_loc;
    std::optional<resource_location> preproc3_loc;

public:
    lsp_context_db2_preprocessor_test()
        : lsp_context_preprocessor_test(
            contents, std::make_shared<mock_parse_lib_provider>(member_list), db2_preprocessor_options())
    {}

    void SetUp() override
    {
        a.analyze();
        preproc2_loc = find_preproc_file("PREPROCESSOR_2.hlasm");
        preproc3_loc = find_preproc_file("PREPROCESSOR_3.hlasm");

        ASSERT_TRUE(preproc2_loc.has_value());
        ASSERT_TRUE(preproc3_loc.has_value());
    }
};

TEST_F(lsp_context_db2_preprocessor_test, go_to_include)
{
    // jump to MEMBER, label A
    EXPECT_EQ(location(position(0, 0), member_loc), a.context().lsp_ctx->definition(source_loc, position(1, 1)));
    // jump to virtual file, label B
    EXPECT_EQ(location(position(0, 0), *preproc2_loc), a.context().lsp_ctx->definition(source_loc, position(2, 1)));
    // jump to virtual file, label C
    EXPECT_EQ(location(position(0, 0), *preproc3_loc), a.context().lsp_ctx->definition(source_loc, position(3, 1)));

    // no jump,  EXEC   SQL  INCLUDE
    EXPECT_EQ(location(position(1, 15), source_loc), a.context().lsp_ctx->definition(source_loc, position(1, 15)));
    // no jump,   EXEC  SQL   INCLUDE
    EXPECT_EQ(location(position(2, 15), source_loc), a.context().lsp_ctx->definition(source_loc, position(2, 15)));
    // no jump, EXEC SQL INCLUDE
    EXPECT_EQ(location(position(3, 15), source_loc), a.context().lsp_ctx->definition(source_loc, position(3, 15)));

    // jump from source to included file
    EXPECT_EQ(location(position(0, 3), source_loc), a.context().lsp_ctx->definition(source_loc, position(1, 29)));
    // jump from source to virtual file
    EXPECT_EQ(location(position(0, 0), *preproc2_loc), a.context().lsp_ctx->definition(source_loc, position(2, 29)));
    // jump from source to virtual file
    EXPECT_EQ(location(position(0, 0), *preproc3_loc), a.context().lsp_ctx->definition(source_loc, position(3, 29)));
}

TEST_F(lsp_context_db2_preprocessor_test, refs_include)
{
    const location_list expected_a_locations {
        location(position(1, 0), source_loc),
        location(position(0, 0), member_loc),
    };
    const location_list expected_b_locations {
        location(position(2, 0), source_loc),
        location(position(0, 0), *preproc2_loc),
    };
    const location_list expected_c_locations {
        location(position(3, 0), source_loc),
        location(position(0, 0), *preproc3_loc),
    };
    const location_list expected_exec_sql_include_locations {
        location(position(1, 7), source_loc),
        location(position(2, 8), source_loc),
        location(position(3, 6), source_loc),
    };
    const location_list expected_member_locations {
        location(position(1, 28), source_loc),
        location(position(0, 3), member_loc),
    };
    const location_list expected_sqlca_locations {
        location(position(2, 28), source_loc),
        location(position(0, 3), *preproc2_loc),
    };
    const location_list expected_sqlda_locations {
        location(position(3, 24), source_loc),
        location(position(0, 3), *preproc3_loc),
    };

    // A reference
    EXPECT_TRUE(has_same_content(expected_a_locations, a.context().lsp_ctx->references(source_loc, position(1, 0))));
    // B reference
    EXPECT_TRUE(has_same_content(expected_b_locations, a.context().lsp_ctx->references(source_loc, position(2, 0))));
    // C reference
    EXPECT_TRUE(has_same_content(expected_c_locations, a.context().lsp_ctx->references(source_loc, position(3, 0))));

    // EXEC SQL INCLUDE reference
    EXPECT_TRUE(has_same_content(
        expected_exec_sql_include_locations, a.context().lsp_ctx->references(source_loc, position(1, 9))));
    // EXEC SQL INCLUDE reference
    EXPECT_TRUE(has_same_content(
        expected_exec_sql_include_locations, a.context().lsp_ctx->references(source_loc, position(1, 19))));
    // EXEC SQL INCLUDE reference
    EXPECT_TRUE(has_same_content(
        expected_exec_sql_include_locations, a.context().lsp_ctx->references(source_loc, position(3, 22))));

    // MEMBER reference
    EXPECT_TRUE(
        has_same_content(expected_member_locations, a.context().lsp_ctx->references(source_loc, position(1, 29))));
    // SQLCA reference
    EXPECT_TRUE(
        has_same_content(expected_member_locations, a.context().lsp_ctx->references(source_loc, position(1, 29))));
    // SQLDA reference
    EXPECT_TRUE(
        has_same_content(expected_member_locations, a.context().lsp_ctx->references(source_loc, position(1, 29))));
}

// Todo: There shouldn't be a reference from ZY as it is commented out -> finish writing the test expectations
// TEST(lsp_context_db2_preprocessor_test, commented_label_reference)
//{
//    std::string input = R"(
//         USING *,12
//         USING SQLDSECT,11
//                                         EXEC  SQL SELECT 1 IN         X
//               TO : --RM                                             ZYX
//               XWV   FROM TABLE WHERE X = :ABCDE
//*
// ZYXWV    DS    F
// ABCDE    DS    F
//    END
//)";
//
//    analyzer a(input, analyzer_options { db2_preprocessor_options {} });
//    a.analyze();
//
//    a.collect_diags();
//    EXPECT_TRUE(a.diags().empty());
//}