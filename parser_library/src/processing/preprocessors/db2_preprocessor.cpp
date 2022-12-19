/*
 * Copyright (c) 2021 Broadcom.
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
#include <cassert>
#include <cctype>
#include <charconv>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <regex>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "diagnostic_consumer.h"
#include "lexing/logical_line.h"
#include "preprocessor_options.h"
#include "preprocessor_utils.h"
#include "processing/preprocessor.h"
#include "range.h"
#include "semantics/range_provider.h"
#include "semantics/source_info_processor.h"
#include "semantics/statement.h"
#include "utils/concat.h"
#include "utils/resource_location.h"
#include "utils/string_operations.h"
#include "workspaces/parse_lib_provider.h"

namespace hlasm_plugin::parser_library::processing {
namespace {
using utils::concat;

using range_adjuster = std::function<range(
    size_t start_lineno, size_t start_column, size_t end_lineno, size_t end_column)>; // todo check for null

// emulates limited variant of alternative operand parser and performs DFHRESP/DFHVALUE substitutions
// recognizes L' attribute, '...' strings and skips end of line comments
template<typename It>
class mini_parser
{
    std::string m_substituted_operands;

    enum class symbol_type : unsigned char
    {
        normal,
        ord_char,
        blank,
        colon,
        quote,
        remark_start,
    };

    static constexpr std::array symbols = []() {
        std::array<symbol_type, std::numeric_limits<unsigned char>::max() + 1> r {};

        r[(unsigned char)'0'] = symbol_type::ord_char;
        r[(unsigned char)'1'] = symbol_type::ord_char;
        r[(unsigned char)'2'] = symbol_type::ord_char;
        r[(unsigned char)'3'] = symbol_type::ord_char;
        r[(unsigned char)'4'] = symbol_type::ord_char;
        r[(unsigned char)'5'] = symbol_type::ord_char;
        r[(unsigned char)'6'] = symbol_type::ord_char;
        r[(unsigned char)'7'] = symbol_type::ord_char;
        r[(unsigned char)'8'] = symbol_type::ord_char;
        r[(unsigned char)'9'] = symbol_type::ord_char;
        r[(unsigned char)'A'] = symbol_type::ord_char;
        r[(unsigned char)'B'] = symbol_type::ord_char;
        r[(unsigned char)'C'] = symbol_type::ord_char;
        r[(unsigned char)'D'] = symbol_type::ord_char;
        r[(unsigned char)'E'] = symbol_type::ord_char;
        r[(unsigned char)'F'] = symbol_type::ord_char;
        r[(unsigned char)'G'] = symbol_type::ord_char;
        r[(unsigned char)'H'] = symbol_type::ord_char;
        r[(unsigned char)'I'] = symbol_type::ord_char;
        r[(unsigned char)'J'] = symbol_type::ord_char;
        r[(unsigned char)'K'] = symbol_type::ord_char;
        r[(unsigned char)'L'] = symbol_type::ord_char;
        r[(unsigned char)'M'] = symbol_type::ord_char;
        r[(unsigned char)'N'] = symbol_type::ord_char;
        r[(unsigned char)'O'] = symbol_type::ord_char;
        r[(unsigned char)'P'] = symbol_type::ord_char;
        r[(unsigned char)'Q'] = symbol_type::ord_char;
        r[(unsigned char)'R'] = symbol_type::ord_char;
        r[(unsigned char)'S'] = symbol_type::ord_char;
        r[(unsigned char)'T'] = symbol_type::ord_char;
        r[(unsigned char)'U'] = symbol_type::ord_char;
        r[(unsigned char)'V'] = symbol_type::ord_char;
        r[(unsigned char)'W'] = symbol_type::ord_char;
        r[(unsigned char)'X'] = symbol_type::ord_char;
        r[(unsigned char)'Y'] = symbol_type::ord_char;
        r[(unsigned char)'Z'] = symbol_type::ord_char;
        r[(unsigned char)'a'] = symbol_type::ord_char;
        r[(unsigned char)'b'] = symbol_type::ord_char;
        r[(unsigned char)'c'] = symbol_type::ord_char;
        r[(unsigned char)'d'] = symbol_type::ord_char;
        r[(unsigned char)'e'] = symbol_type::ord_char;
        r[(unsigned char)'f'] = symbol_type::ord_char;
        r[(unsigned char)'g'] = symbol_type::ord_char;
        r[(unsigned char)'h'] = symbol_type::ord_char;
        r[(unsigned char)'i'] = symbol_type::ord_char;
        r[(unsigned char)'j'] = symbol_type::ord_char;
        r[(unsigned char)'k'] = symbol_type::ord_char;
        r[(unsigned char)'l'] = symbol_type::ord_char;
        r[(unsigned char)'m'] = symbol_type::ord_char;
        r[(unsigned char)'n'] = symbol_type::ord_char;
        r[(unsigned char)'o'] = symbol_type::ord_char;
        r[(unsigned char)'p'] = symbol_type::ord_char;
        r[(unsigned char)'q'] = symbol_type::ord_char;
        r[(unsigned char)'r'] = symbol_type::ord_char;
        r[(unsigned char)'s'] = symbol_type::ord_char;
        r[(unsigned char)'t'] = symbol_type::ord_char;
        r[(unsigned char)'u'] = symbol_type::ord_char;
        r[(unsigned char)'v'] = symbol_type::ord_char;
        r[(unsigned char)'w'] = symbol_type::ord_char;
        r[(unsigned char)'x'] = symbol_type::ord_char;
        r[(unsigned char)'y'] = symbol_type::ord_char;
        r[(unsigned char)'z'] = symbol_type::ord_char;
        r[(unsigned char)'_'] = symbol_type::ord_char;
        r[(unsigned char)'@'] = symbol_type::ord_char;
        r[(unsigned char)'$'] = symbol_type::ord_char;
        r[(unsigned char)'#'] = symbol_type::ord_char;
        r[(unsigned char)' '] = symbol_type::blank;
        r[(unsigned char)':'] = symbol_type::colon;
        r[(unsigned char)'\''] = symbol_type::quote;
        r[(unsigned char)'\"'] = symbol_type::quote;
        r[(unsigned char)'-'] = symbol_type::remark_start;

        return r;
    }();

    template<typename T>
    std::true_type same_line_detector(const T& t, decltype(t.same_line(t)) = false);
    std::false_type same_line_detector(...);

    bool same_line(It l, It r)
    {
        if constexpr (decltype(same_line_detector(l))::value)
            return l.same_line(r);
        else
            return true;
    }

    It skip_to_string_end(It b, It e)
    {
        if (b == e)
            return b;

        unsigned char q = *b;
        while (++b != e)
            if (q == *b)
                break;

        return b;
    }

    It skip_to_next_line(It b, It e)
    {
        if (b == e)
            return b;

        auto skip_line = b;
        while (skip_line != e && same_line(b, skip_line))
            ++skip_line;

        return skip_line;
    }

public:
    const std::string& operands() const& { return m_substituted_operands; }
    std::string operands() && { return std::move(m_substituted_operands); }

    std::vector<semantics::preproc_details::name_range> parse_and_substitute(
        It b, It e, size_t lineno, const range_adjuster& r_adj)
    {
        enum class consuming_state
        {
            NON_CONSUMING,
            PREPARE_TO_CONSUME,
            CONSUMING,
            TRAIL,
            QUOTE,
        };

        It op_start_it = b;
        consuming_state next_state = consuming_state::NON_CONSUMING;
        std::vector<semantics::preproc_details::name_range> operands;

        const auto op_inserter = [&operands, &lineno, &r_adj, orig_it = b](It start, It end) {
            std::string op = std::string(start, end);
            operands.emplace_back(semantics::preproc_details::name_range(std::string(start, end),
                r_adj(lineno, std::distance(orig_it, start), lineno, std::distance(orig_it, end))));
        };

        while (b != e)
        {
            const auto state = std::exchange(next_state, consuming_state::NON_CONSUMING);
            const char c = *b;
            const symbol_type s = symbols[(unsigned char)c];

            switch (s)
            {
                case symbol_type::ord_char:
                    if (state == consuming_state::PREPARE_TO_CONSUME)
                    {
                        op_start_it = b;
                        next_state = consuming_state::CONSUMING;
                    }
                    else if (state == consuming_state::CONSUMING)
                        next_state = consuming_state::CONSUMING;

                    break;

                case symbol_type::colon:
                    if (state == consuming_state::PREPARE_TO_CONSUME || state == consuming_state::TRAIL)
                        break;

                    if (state == consuming_state::CONSUMING)
                    {
                        op_inserter(op_start_it, b);
                        break;
                    }

                    next_state = consuming_state::PREPARE_TO_CONSUME;
                    break;

                case symbol_type::blank:

                    if (state == consuming_state::CONSUMING)
                    {
                        op_inserter(op_start_it, b);
                        next_state = consuming_state::TRAIL;
                        break;
                    }

                    next_state = state;

                    break;

                case symbol_type::quote:
                    if (state == consuming_state::CONSUMING)
                        op_inserter(op_start_it, b);

                    if (b = skip_to_string_end(b, e); b == e)
                        goto done;

                    break;

                case symbol_type::remark_start:
                    if (state == consuming_state::CONSUMING)
                        op_inserter(op_start_it, b);

                    if (auto n = std::next(b); n != e && *n == '-')
                    {
                        b = skip_to_next_line(n, e);
                        next_state = state;
                        continue;
                    }

                    break;

                case symbol_type::normal:
                    if (state == consuming_state::CONSUMING)
                        op_inserter(op_start_it, b);
                    break;

                default:
                    assert(false);
                    break;
            }

            ++b;
        }

        if (next_state == consuming_state::CONSUMING)
            op_inserter(op_start_it, b);

    done:
        return operands;
    }
};

class db2_preprocessor final : public preprocessor // TODO Take DBCS into account
{
    semantics::preproc_details m_preproc_details;
    std::vector<semantics::preproc_details::name_range> m_args;
    lexing::logical_line m_logical_line;
    std::string m_version;
    bool m_conditional;
    library_fetcher m_libs;
    diagnostic_op_consumer* m_diags = nullptr;
    std::vector<document_line> m_result;
    bool m_source_translated = false;
    semantics::source_info_processor& m_src_proc;

    void push_sql_version_data()
    {
        assert(!m_version.empty());

        constexpr auto version_chunk = (size_t)32;
        if (m_version.size() <= version_chunk)
        {
            m_result.emplace_back(replaced_line { "SQLVERSP DC    CL4'VER.' VERSION-ID PREFIX\n" });
            m_result.emplace_back(replaced_line { concat("SQLVERD1 DC    CL64'", m_version, "'        VERSION-ID\n") });
        }
        else
        {
            m_result.emplace_back(replaced_line { "SQLVERS  DS    CL68      VERSION-ID\n" });
            m_result.emplace_back(replaced_line { "         ORG   SQLVERS+0\n" });
            m_result.emplace_back(replaced_line { "SQLVERSP DC    CL4'VER.' VERS-ID PREFIX\n" });

            for (auto [version, i] = std::pair(std::string_view(m_version), 1); !version.empty();
                 version.remove_prefix(std::min(version.size(), version_chunk)), ++i)
            {
                auto i_str = std::to_string(i);
                m_result.emplace_back(replaced_line { concat("SQLVERD",
                    i_str,
                    " DC    CL32'",
                    version.substr(0, version_chunk),
                    "'    VERS-ID PART-",
                    i_str,
                    "\n") });
            }
        }
    }

    void push_sql_working_storage()
    {
        if (!m_version.empty())
            push_sql_version_data();

        m_result.emplace_back(replaced_line { "***$$$ SQL WORKING STORAGE                      \n" });
        m_result.emplace_back(replaced_line { "SQLDSIZ  DC    A(SQLDLEN) SQLDSECT SIZE         \n" });
        m_result.emplace_back(replaced_line { "SQLDSECT DSECT                                  \n" });
        m_result.emplace_back(replaced_line { "SQLTEMP  DS    CL128     TEMPLATE               \n" });
        m_result.emplace_back(replaced_line { "DSNTEMP  DS    F         INT SCROLL VALUE       \n" });
        m_result.emplace_back(replaced_line { "DSNTMP2  DS    PL16      DEC SCROLL VALUE       \n" });
        m_result.emplace_back(replaced_line { "DSNNROWS DS    F         MULTI-ROW N-ROWS VALUE \n" });
        m_result.emplace_back(replaced_line { "DSNNTYPE DS    H         MULTI-ROW N-ROWS TYPE  \n" });
        m_result.emplace_back(replaced_line { "DSNNLEN  DS    H         MULTI-ROW N-ROWS LENGTH\n" });
        m_result.emplace_back(replaced_line { "DSNPARMS DS    4F        DSNHMLTR PARM LIST     \n" });
        m_result.emplace_back(replaced_line { "DSNPNM   DS    CL386     PROCEDURE NAME         \n" });
        m_result.emplace_back(replaced_line { "DSNCNM   DS    CL128     CURSOR NAME            \n" });
        m_result.emplace_back(replaced_line { "SQL_FILE_READ      EQU 2                        \n" });
        m_result.emplace_back(replaced_line { "SQL_FILE_CREATE    EQU 8                        \n" });
        m_result.emplace_back(replaced_line { "SQL_FILE_OVERWRITE EQU 16                       \n" });
        m_result.emplace_back(replaced_line { "SQL_FILE_APPEND    EQU 32                       \n" });
        m_result.emplace_back(replaced_line { "         DS    0D                               \n" });
        m_result.emplace_back(replaced_line { "SQLPLIST DS    F                                \n" });
        m_result.emplace_back(replaced_line { "SQLPLLEN DS    H         PLIST LENGTH           \n" });
        m_result.emplace_back(replaced_line { "SQLFLAGS DS    XL2       FLAGS                  \n" });
        m_result.emplace_back(replaced_line { "SQLCTYPE DS    H         CALL-TYPE              \n" });
        m_result.emplace_back(replaced_line { "SQLPROGN DS    CL8       PROGRAM NAME           \n" });
        m_result.emplace_back(replaced_line { "SQLTIMES DS    CL8       TIMESTAMP              \n" });
        m_result.emplace_back(replaced_line { "SQLSECTN DS    H         SECTION                \n" });
        m_result.emplace_back(replaced_line { "SQLCODEP DS    A         CODE POINTER           \n" });
        m_result.emplace_back(replaced_line { "SQLVPARM DS    A         VPARAM POINTER         \n" });
        m_result.emplace_back(replaced_line { "SQLAPARM DS    A         AUX PARAM PTR          \n" });
        m_result.emplace_back(replaced_line { "SQLSTNM7 DS    H         PRE_V8 STATEMENT NUMBER\n" });
        m_result.emplace_back(replaced_line { "SQLSTYPE DS    H         STATEMENT TYPE         \n" });
        m_result.emplace_back(replaced_line { "SQLSTNUM DS    F         STATEMENT NUMBER       \n" });
        m_result.emplace_back(replaced_line { "SQLFLAG2 DS    H         internal flags         \n" });
        m_result.emplace_back(replaced_line { "SQLRSRVD DS    CL18      RESERVED               \n" });
        m_result.emplace_back(replaced_line { "SQLPVARS DS    CL8,F,2H,0CL44                   \n" });
        m_result.emplace_back(replaced_line { "SQLAVARS DS    CL8,F,2H,0CL44                   \n" });
        m_result.emplace_back(replaced_line { "         DS    0D                               \n" });
        m_result.emplace_back(replaced_line { "SQLDLEN  EQU   *-SQLDSECT                       \n" });
    }

    void inject_SQLCA()
    {
        m_result.emplace_back(replaced_line { "***$$$ SQLCA                          \n" });
        m_result.emplace_back(replaced_line { "SQLCA    DS    0F                     \n" });
        m_result.emplace_back(replaced_line { "SQLCAID  DS    CL8      ID            \n" });
        m_result.emplace_back(replaced_line { "SQLCABC  DS    F        BYTE COUNT    \n" });
        m_result.emplace_back(replaced_line { "SQLCODE  DS    F        RETURN CODE   \n" });
        m_result.emplace_back(replaced_line { "SQLERRM  DS    H,CL70   ERR MSG PARMS \n" });
        m_result.emplace_back(replaced_line { "SQLERRP  DS    CL8      IMPL-DEPENDENT\n" });
        m_result.emplace_back(replaced_line { "SQLERRD  DS    6F                     \n" });
        m_result.emplace_back(replaced_line { "SQLWARN  DS    0C       WARNING FLAGS \n" });
        m_result.emplace_back(replaced_line { "SQLWARN0 DS    C'W' IF ANY            \n" });
        m_result.emplace_back(replaced_line { "SQLWARN1 DS    C'W' = WARNING         \n" });
        m_result.emplace_back(replaced_line { "SQLWARN2 DS    C'W' = WARNING         \n" });
        m_result.emplace_back(replaced_line { "SQLWARN3 DS    C'W' = WARNING         \n" });
        m_result.emplace_back(replaced_line { "SQLWARN4 DS    C'W' = WARNING         \n" });
        m_result.emplace_back(replaced_line { "SQLWARN5 DS    C'W' = WARNING         \n" });
        m_result.emplace_back(replaced_line { "SQLWARN6 DS    C'W' = WARNING         \n" });
        m_result.emplace_back(replaced_line { "SQLWARN7 DS    C'W' = WARNING         \n" });
        m_result.emplace_back(replaced_line { "SQLEXT   DS    0CL8                   \n" });
        m_result.emplace_back(replaced_line { "SQLWARN8 DS    C                      \n" });
        m_result.emplace_back(replaced_line { "SQLWARN9 DS    C                      \n" });
        m_result.emplace_back(replaced_line { "SQLWARNA DS    C                      \n" });
        m_result.emplace_back(replaced_line { "SQLSTATE DS    CL5                    \n" });
        m_result.emplace_back(replaced_line { "***$$$\n" });
    }
    void inject_SQLDA()
    {
        m_result.emplace_back(replaced_line { "***$$$ SQLDA                                            \n" });
        m_result.emplace_back(replaced_line { "SQLTRIPL EQU    C'3'                                    \n" });
        m_result.emplace_back(replaced_line { "SQLDOUBL EQU    C'2'                                    \n" });
        m_result.emplace_back(replaced_line { "SQLSINGL EQU    C' '                                    \n" });
        m_result.emplace_back(replaced_line { "*                                                       \n" });
        m_result.emplace_back(replaced_line { "         SQLSECT SAVE                                   \n" });
        m_result.emplace_back(replaced_line { "*                                                       \n" });
        m_result.emplace_back(replaced_line { "SQLDA    DSECT                                          \n" });
        m_result.emplace_back(replaced_line { "SQLDAID  DS    CL8      ID                              \n" });
        m_result.emplace_back(replaced_line { "SQLDABC  DS    F        BYTE COUNT                      \n" });
        m_result.emplace_back(replaced_line { "SQLN     DS    H        COUNT SQLVAR/SQLVAR2 ENTRIES    \n" });
        m_result.emplace_back(replaced_line { "SQLD     DS    H        COUNT VARS (TWICE IF USING BOTH)\n" });
        m_result.emplace_back(replaced_line { "*                                                       \n" });
        m_result.emplace_back(replaced_line { "SQLVAR   DS    0F       BEGIN VARS                      \n" });
        m_result.emplace_back(replaced_line { "SQLVARN  DSECT ,        NTH VARIABLE                    \n" });
        m_result.emplace_back(replaced_line { "SQLTYPE  DS    H        DATA TYPE CODE                  \n" });
        m_result.emplace_back(replaced_line { "SQLLEN   DS    0H       LENGTH                          \n" });
        m_result.emplace_back(replaced_line { "SQLPRCSN DS    X        DEC PRECISION                   \n" });
        m_result.emplace_back(replaced_line { "SQLSCALE DS    X        DEC SCALE                       \n" });
        m_result.emplace_back(replaced_line { "SQLDATA  DS    A        ADDR OF VAR                     \n" });
        m_result.emplace_back(replaced_line { "SQLIND   DS    A        ADDR OF IND                     \n" });
        m_result.emplace_back(replaced_line { "SQLNAME  DS    H,CL30   DESCRIBE NAME                   \n" });
        m_result.emplace_back(replaced_line { "SQLVSIZ  EQU   *-SQLDATA                                \n" });
        m_result.emplace_back(replaced_line { "SQLSIZV  EQU   *-SQLVARN                                \n" });
        m_result.emplace_back(replaced_line { "*                                                       \n" });
        m_result.emplace_back(replaced_line { "SQLDA    DSECT                                          \n" });
        m_result.emplace_back(replaced_line { "SQLVAR2  DS     0F      BEGIN EXTENDED FIELDS OF VARS   \n" });
        m_result.emplace_back(replaced_line { "SQLVAR2N DSECT  ,       EXTENDED FIELDS OF NTH VARIABLE \n" });
        m_result.emplace_back(replaced_line { "SQLLONGL DS     F       LENGTH                          \n" });
        m_result.emplace_back(replaced_line { "SQLRSVDL DS     F       RESERVED                        \n" });
        m_result.emplace_back(replaced_line { "SQLDATAL DS     A       ADDR OF LENGTH IN BYTES         \n" });
        m_result.emplace_back(replaced_line { "SQLTNAME DS     H,CL30  DESCRIBE NAME                   \n" });
        m_result.emplace_back(replaced_line { "*                                                       \n" });
        m_result.emplace_back(replaced_line { "         SQLSECT RESTORE                                \n" });
        m_result.emplace_back(replaced_line { "***$$$\n" });
    }
    void inject_SQLSECT()
    {
        m_result.emplace_back(replaced_line { "         MACRO                          \n" });
        m_result.emplace_back(replaced_line { "         SQLSECT &TYPE                  \n" });
        m_result.emplace_back(replaced_line { "         GBLC  &SQLSECT                 \n" });
        m_result.emplace_back(replaced_line { "         AIF ('&TYPE' EQ 'RESTORE').REST\n" });
        m_result.emplace_back(replaced_line { "&SQLSECT SETC  '&SYSECT'                \n" });
        m_result.emplace_back(replaced_line { "         MEXIT                          \n" });
        m_result.emplace_back(replaced_line { ".REST    ANOP                           \n" });
        m_result.emplace_back(replaced_line { "&SQLSECT CSECT                          \n" });
        m_result.emplace_back(replaced_line { "         MEND                           \n" });
    }

    void process_include(std::string_view operands, size_t lineno)
    {
        auto operands_upper = context::to_upper_copy(std::string(operands));

        if (operands_upper == "SQLCA")
        {
            inject_SQLCA();
            return;
        }
        if (operands_upper == "SQLDA")
        {
            inject_SQLDA();
            return;
        }
        m_result.emplace_back(replaced_line { "***$$$\n" });

        std::optional<std::pair<std::string, utils::resource::resource_location>> include_member;
        if (m_libs)
            include_member = m_libs(operands_upper);
        if (!include_member.has_value())
        {
            if (m_diags)
                m_diags->add_diagnostic(diagnostic_op::error_DB002(range(position(lineno, 0)), operands));
            return;
        }

        auto& [include_mem_text, include_mem_loc] = *include_member;
        document d(include_mem_text);
        d.convert_to_replaced();
        generate_replacement(d.begin(), d.end(), false);
        append_included_member(std::make_unique<included_member_details>(included_member_details {
            std::move(operands_upper), std::move(include_mem_text), std::move(include_mem_loc) }));
    }
    static size_t consume_words_and_trim(
        std::string_view& l, std::initializer_list<std::string_view> words, bool tolerate_no_space_at_end = false)
    {
        size_t consumed_words_spread = 0;
        size_t last_trim = 0;
        const auto init_l = l;
        for (const auto& w : words)
        {
            if (auto consumed = utils::consume(l, w); !consumed)
            {
                l = init_l; // all or nothing
                return 0;
            }
            else
                consumed_words_spread += consumed + last_trim;


            if (last_trim = utils::trim_left(l); !last_trim)
            {
                if (tolerate_no_space_at_end && l.empty() && &w == words.end() - 1)
                    return consumed_words_spread;
                l = init_l; // all or nothing
                return 0;
            }
        }

        return consumed_words_spread;
    }

    static bool is_end(std::string_view s)
    {
        if (!utils::consume(s, "END"))
            return false;

        if (s.empty() || s.front() == ' ')
            return true;

        return false;
    }

    static std::string_view create_line_preview(std::string_view input)
    {
        const auto begin_offset = lexing::default_ictl.begin - 1;
        using namespace std::literals;
        if (input.size() < begin_offset)
            return {};

        input = input.substr(begin_offset, lexing::default_ictl.end - begin_offset);

        if (const auto rn = input.find_first_of("\r\n"sv); rn != std::string_view::npos)
            input = input.substr(0, rn);

        return input;
    }

    static bool ignore_line(std::string_view s) { return s.empty() || s.front() == '*' || s.substr(0, 2) == ".*"; }

    static semantics::preproc_details::name_range extract_label(
        std::string_view& s, size_t lineno, const range_adjuster& r_adj)
    {
        auto label = utils::next_nonblank_sequence(s);
        if (!label.length())
            return {};

        s.remove_prefix(label.length());

        return semantics::preproc_details::name_range(std::string(label), r_adj(lineno, 0, lineno, label.length()));
    }

    enum class line_type
    {
        ignore,
        exec_sql,
        include,
        sql_type
    };

    static std::pair<line_type, semantics::preproc_details::name_range> consume_instruction(
        std::string_view& line_preview, size_t lineno, size_t instr_column_start, const range_adjuster& r_adj)
    {
        static const std::pair<line_type, semantics::preproc_details::name_range> ignore(line_type::ignore, {});

        if (line_preview.empty())
            return ignore;

        const auto consume_and_create = [lineno, instr_column_start, &r_adj](std::string_view& line_preview,
                                            line_type line,
                                            std::initializer_list<std::string_view> words_to_consume,
                                            std::string line_id) {
            if (auto consumed_size = consume_words_and_trim(line_preview, words_to_consume))
                return std::pair<line_type, semantics::preproc_details::name_range>(line,
                    semantics::preproc_details::name_range(
                        line_id, r_adj(lineno, instr_column_start, lineno, instr_column_start + consumed_size)));
            return ignore;
        };

        switch (line_preview.front())
        {
            case 'E':
                return consume_and_create(line_preview, line_type::exec_sql, { "EXEC", "SQL" }, "EXEC SQL");

            case 'S':
                return consume_and_create(line_preview, line_type::sql_type, { "SQL", "TYPE", "IS" }, "SQL TYPE IS");

            default:
                return ignore;
        }
    }

    void add_ds_line(std::string_view label, std::string_view label_suffix, std::string_view type, bool align = true)
    {
        m_result.emplace_back(replaced_line { concat(label,
            label_suffix,
            std::string(
                align && label.size() + label_suffix.size() < 8 ? 8 - (label.size() + label_suffix.size()) : 0, ' '),
            " DS ",
            std::string(align ? 2 + (type.front() != '0') : 0, ' '),
            type,
            "\n") });
    };

    struct lob_info_t
    {
        unsigned long long scale;
        unsigned long long limit;
        std::string prefix;
    };

    static lob_info_t lob_info(char type, char scale)
    {
        lob_info_t result;
        switch (scale)
        {
            case 'K':
                result.scale = 1024ULL;
                break;
            case 'M':
                result.scale = 1024ULL * 1024;
                break;
            case 'G':
                result.scale = 1024ULL * 1024 * 1024;
                break;
            default:
                result.scale = 1ULL;
                break;
        }
        switch (type)
        {
            case 'B':
                result.limit = 65535;
                result.prefix = "CL";
                break;
            case 'C':
                result.limit = 65535;
                result.prefix = "CL";
                break;
            case 'D':
                result.limit = 65534;
                result.prefix = "GL";
                break;
        }
        return result;
    }

    bool handle_lob(const std::regex& pattern, std::string_view label, std::string_view operands)
    {
        std::match_results<std::string_view::const_iterator> match;
        if (!std::regex_match(operands.cbegin(), operands.cend(), match, pattern))
            return false;

        switch ((match[4].matched ? match[4] : match[1]).second[-1])
        {
            case 'E': // ..._FILE
                add_ds_line(label, "", "0FL4");
                add_ds_line(label, "_NAME_LENGTH", "FL4", false);
                add_ds_line(label, "_DATA_LENGTH", "FL4", false);
                add_ds_line(label, "_FILE_OPTIONS", "FL4", false);
                add_ds_line(label, "_NAME", "CL255", false);
                break;

            case 'R': // ..._LOCATOR
                add_ds_line(label, "", "FL4");
                break;

            default: {
                const auto li = lob_info(*match[1].first, match[3].matched ? *match[3].first : 0);
                unsigned long long len;
                std::from_chars(std::to_address(match[2].first), std::to_address(match[2].second), len);
                len *= li.scale;

                add_ds_line(label, "", "0FL4");
                add_ds_line(label, "_LENGTH", "FL4", false);
                add_ds_line(label, "_DATA", li.prefix + std::to_string(len <= li.limit ? len : li.limit), false);
                if (len > li.limit)
                    m_result.emplace_back(replaced_line { concat(" ORG   *+(",
                        // there seems be this strange artificial limit
                        std::min(len - li.limit, 1073676289ULL),
                        ")\n") });
                break;
            }
        }
        return true;
    };

    bool process_sql_type_operands(std::string_view operands, std::string_view label)
    {
        if (operands.size() < 2)
            return false;

        // keep the capture groups in sync
        static const auto xml_type = std::regex(
            "XML[ ]+AS[ ]+"
            "(?:"
            "(BINARY[ ]+LARGE[ ]+OBJECT|BLOB|CHARACTER[ ]+LARGE[ ]+OBJECT|CHAR[ ]+LARGE[ ]+OBJECT|CLOB|DBCLOB)"
            "[ ]+([[:digit:]]{1,9})([KMG])?"
            "|"
            "(BLOB_FILE|CLOB_FILE|DBCLOB_FILE)"
            ")"
            "(?: .*)?");
        static const auto lob_type = std::regex(
            "(?:"
            "(BINARY[ ]+LARGE[ ]+OBJECT|BLOB|CHARACTER[ ]+LARGE[ ]+OBJECT|CHAR[ ]+LARGE[ ]+OBJECT|CLOB|DBCLOB)"
            "[ ]+([[:digit:]]{1,9})([KMG])?"
            "|"
            "(BLOB_FILE|CLOB_FILE|DBCLOB_FILE|BLOB_LOCATOR|CLOB_LOCATOR|DBCLOB_LOCATOR)"
            ")"
            "(?: .*)?");

        static const auto table_like =
            std::regex("TABLE[ ]+LIKE[ ]+('(?:[^']|'')+'|(?:[^']|'')+)[ ]+AS[ ]+LOCATOR(?: .*)?");

        switch (operands[0])
        {
            case 'R':
                switch (operands[1])
                {
                    case 'E':
                        if (!consume_words_and_trim(operands, { "RESULT_SET_LOCATOR", "VARYING" }, true))
                            break;
                        add_ds_line(label, "", "FL4");
                        return true;

                    case 'O':
                        if (!consume_words_and_trim(operands, { "ROWID" }, true))
                            break;
                        add_ds_line(label, "", "H,CL40");
                        return true;
                }
                break;

            case 'T':
                if (!std::regex_match(operands.begin(), operands.end(), table_like))
                    break;
                add_ds_line(label, "", "FL4");
                return true;

            case 'X':
                return handle_lob(xml_type, label, operands);

            case 'B':
            case 'C':
            case 'D':
                return handle_lob(lob_type, label, operands);
        }
        return false;
    }

    std::string process_regular_line(
        size_t lineno, std::string_view label, size_t first_line_skipped, const range_adjuster& r_adj)
    {
        std::string operands;

        if (!label.empty())
            m_result.emplace_back(replaced_line { concat(label, " DS 0H\n") });

        m_result.emplace_back(replaced_line { "***$$$\n" });

        for (const auto& segment : m_logical_line.segments)
        {
            std::string this_line(segment.line);

            auto operand_remark_part = segment.code;
            if (first_line_skipped)
            {
                const auto appended_line_size = segment.line.size();
                operand_remark_part.remove_prefix(first_line_skipped);
                if (!label.empty())
                    this_line.replace(this_line.size() - appended_line_size,
                        label.size(),
                        label.size(),
                        ' '); // mask out any label-like characters
                this_line[this_line.size() - appended_line_size] = '*';
            }

            this_line.append("\n");
            m_result.emplace_back(replaced_line { std::move(this_line) });

            utils::trim_right(operand_remark_part);
            auto remark_start = operand_remark_part.find("--");

            if (auto operand_part = operand_remark_part.substr(0, remark_start); !operand_part.empty())
            {
                utils::trim_right(operand_part);
                operands.append(operand_part);
                m_preproc_details.operands.items.emplace_back(std::string(operand_part),
                    r_adj(lineno, first_line_skipped, lineno, first_line_skipped + operand_part.length()));
            }

            if (remark_start != std::string_view::npos)
                m_preproc_details.remarks.items.emplace_back(r_adj(lineno,
                    first_line_skipped + remark_start,
                    lineno,
                    first_line_skipped + operand_remark_part.length()));

            lineno++;
            first_line_skipped = 0;
        }

        return operands;
    }

    std::string process_sql_type_line(size_t lineno, size_t first_line_skipped, const range_adjuster& r_adj)
    {
        std::string operands;
        m_result.emplace_back(replaced_line { "***$$$\n" });
        m_result.emplace_back(replaced_line {
            concat("*", m_logical_line.segments.front().code.substr(0, lexing::default_ictl.end - 1), "\n") });

        for (const auto& segment : m_logical_line.segments)
        {
            auto ops = segment.code.substr(first_line_skipped);
            utils::trim_right(ops);
            operands.append(ops);
            m_preproc_details.operands.items.emplace_back(
                std::string(ops), r_adj(lineno, first_line_skipped, lineno, first_line_skipped + ops.length()));
            lineno++;
            first_line_skipped = 0;
        }

        m_result.emplace_back(replaced_line { "***$$$\n" });

        return operands;
    }

    std::tuple<std::pair<line_type, semantics::preproc_details::name_range>,
        size_t,
        semantics::preproc_details::name_range>
    check_line(std::string_view input, size_t lineno, const range_adjuster& r_adj)
    {
        static const std::tuple<std::pair<line_type, semantics::preproc_details::name_range>,
            size_t,
            semantics::preproc_details::name_range>
            ignore({ line_type::ignore, {} }, 0, {});
        std::string_view line_preview = create_line_preview(input);

        if (ignore_line(line_preview))
            return ignore;

        size_t first_line_skipped = line_preview.size();
        semantics::preproc_details::name_range label = extract_label(line_preview, lineno, r_adj);

        auto trimmed = utils::trim_left(line_preview);
        if (!trimmed)
            return ignore;

        if (is_end(line_preview))
        {
            push_sql_working_storage();

            return ignore;
        }

        auto instruction_info = consume_instruction(line_preview, lineno, label.r.end.column + trimmed, r_adj);
        const auto& instruction_type = instruction_info.first;
        if (instruction_type == line_type::ignore)
            return ignore;

        if (!line_preview.empty())
            first_line_skipped = line_preview.data() - input.data();

        return { instruction_info, first_line_skipped, label };
    }

    line_type process_nonempty_line(size_t lineno,
        bool include_allowed,
        line_type instruction,
        size_t first_line_skipped,
        std::string_view label,
        const range_adjuster& r_adj)
    {
        if (m_logical_line.continuation_error && m_diags)
            m_diags->add_diagnostic(diagnostic_op::error_DB001(range(position(lineno, 0))));

        switch (instruction)
        {
            case line_type::exec_sql: {
                std::string operands = process_regular_line(lineno, label, first_line_skipped, r_adj);
                auto operands_view = std::string_view(operands);
                if (consume_words_and_trim(operands_view, { "INCLUDE" }))
                {
                    instruction = line_type::include;
                    if (utils::trim_right(operands_view); include_allowed)
                        process_include(operands_view, lineno);
                    else if (m_diags)
                        m_diags->add_diagnostic(diagnostic_op::error_DB003(range(position(lineno, 0)), operands_view));
                }
                else
                {
                    if (sql_has_codegen(operands))
                    {
                        mini_parser<lexing::logical_line::const_iterator> p;
                        m_args = p.parse_and_substitute(m_logical_line.begin(), m_logical_line.end(), lineno, r_adj);

                        generate_sql_code_mock(m_args.size());
                    }
                    m_result.emplace_back(replaced_line { "***$$$\n" });
                }

                break;
            }
            case line_type::sql_type:
                std::string operands = process_sql_type_line(lineno, first_line_skipped, r_adj);
                // DB2 preprocessor exhibits strange behavior when SQL TYPE line is continued
                if (m_logical_line.segments.size() > 1 && m_diags)
                    m_diags->add_diagnostic(diagnostic_op::error_DB005(range(position(lineno, 0))));
                if (label.empty())
                    label = " "; // best matches the observed behavior
                if (!process_sql_type_operands(operands, label) && m_diags)
                    m_diags->add_diagnostic(diagnostic_op::error_DB004(range(position(lineno, 0))));
                break;
        }

        return instruction;
    }

    static bool sql_has_codegen(std::string_view sql)
    {
        // handles only the most obvious cases (imprecisely)
        static const auto no_code_statements =
            std::regex("(?:DECLARE|WHENEVER|BEGIN[ ]+DECLARE[ ]+SECTION|END[ ]+DECLARE[ ]+SECTION)(?: .*)?",
                std::regex_constants::icase);
        return !std::regex_match(sql.begin(), sql.end(), no_code_statements);
    }
    void generate_sql_code_mock(size_t in_params)
    {
        // this function generates semi-realistic sql statement replacement code, because people do strange things...
        // <arguments> output parameters
        m_result.emplace_back(replaced_line { "         BRAS  15,*+56                     \n" });
        m_result.emplace_back(replaced_line { "         DC    H'0',X'0000',H'0'           \n" });
        m_result.emplace_back(replaced_line { "         DC    XL8'0000000000000000'       \n" });
        m_result.emplace_back(replaced_line { "         DC    XL8'0000000000000000',H'0'  \n" });
        m_result.emplace_back(replaced_line { "         DC    H'0,0,0',X'0000',H'0',9H'0' \n" });
        m_result.emplace_back(replaced_line { "         MVC   SQLPLLEN(24),0(15)          \n" });
        m_result.emplace_back(replaced_line { "         MVC   SQLSTNM7(28),24(15)         \n" });
        m_result.emplace_back(replaced_line { "         LA    15,SQLCA                    \n" });
        m_result.emplace_back(replaced_line { "         ST    15,SQLCODEP                 \n" });

        if (in_params == 0)
        {
            m_result.emplace_back(replaced_line { "         MVC   SQLVPARM,=XL4'00000000'     \n" });
        }
        else
        {
            m_result.emplace_back(replaced_line { "         LA    14,SQLPVARS+16              \n" });
            for (size_t i = 0; i < in_params; ++i)
            {
                if (i > 0)
                    m_result.emplace_back(replaced_line { "         LA    14,44(,14)                  \n" });
                m_result.emplace_back(replaced_line { "         LA    15,0                        \n" });
                m_result.emplace_back(replaced_line { "         ST    15,4(,14)                   \n" });
                m_result.emplace_back(replaced_line { "         MVC   0(2,14),=X'0000'            \n" });
                m_result.emplace_back(replaced_line { "         MVC   2(2,14),=H'0'               \n" });
                m_result.emplace_back(replaced_line { "         SLR   15,15                       \n" });
                m_result.emplace_back(replaced_line { "         ST    15,8(,14)                   \n" });
                m_result.emplace_back(replaced_line { "         SLR   15,15                       \n" });
                m_result.emplace_back(replaced_line { "         ST    15,12(,14)                  \n" });
            }
            m_result.emplace_back(replaced_line { "         LA    14,SQLPVARS                   \n" });
            m_result.emplace_back(replaced_line { "         MVC   0(8,14),=XL8'0000000000000000'\n" });
            m_result.emplace_back(replaced_line { "         MVC   8(4,14),=F'0'                 \n" });
            m_result.emplace_back(replaced_line { "         MVC   12(2,14),=H'0'                \n" });
            m_result.emplace_back(replaced_line { "         MVC   14(2,14),=H'0'                \n" });
            m_result.emplace_back(replaced_line { "         ST    14,SQLVPARM                   \n" });
        }
        m_result.emplace_back(replaced_line { "         MVC   SQLAPARM,=XL4'00000000'     \n" });

        m_result.emplace_back(replaced_line { "         LA    1,SQLPLLEN                  \n" });
        m_result.emplace_back(replaced_line { "         ST    1,SQLPLIST                  \n" });
        m_result.emplace_back(replaced_line { "         OI    SQLPLIST,X'80'              \n" });
        m_result.emplace_back(replaced_line { "         LA    1,SQLPLIST                  \n" });
        m_result.emplace_back(replaced_line { "         L     15,=V(DSNHLI)               \n" });
        m_result.emplace_back(replaced_line { "         BALR  14,15                       \n" });
    }

    void skip_process(line_iterator& it, line_iterator end)
    {
        static constexpr std::string_view PROCESS_LITERAL = "*PROCESS";
        for (; it != end; ++it)
        {
            const auto text = it->text();
            if (text.size() < PROCESS_LITERAL.size())
                break;
            if (text.size() > PROCESS_LITERAL.size() && text[PROCESS_LITERAL.size()] != ' ')
                break;
            if (!std::equal(
                    PROCESS_LITERAL.begin(), PROCESS_LITERAL.end(), text.begin(), [](unsigned char l, unsigned char r) {
                        return l == ::toupper(r);
                    }))
                break;

            m_result.push_back(*it);
        }
    }

    void generate_replacement(line_iterator it, line_iterator end, bool include_allowed)
    {
        bool skip_continuation = false;
        while (it != end)
        {
            const auto text = it->text();
            if (skip_continuation)
            {
                m_result.emplace_back(*it++);
                skip_continuation = is_continued(text);
                continue;
            }

            size_t lineno = it->lineno().value_or(0); // TODO: needs to be addressed for chained preprocessors
            m_logical_line.clear();
            auto backup_it = it;

            it = extract_nonempty_logical_line(m_logical_line, it, end, lexing::default_ictl);

            auto rp = semantics::range_provider(
                range(
                    position(lineno, 0), position(lineno, std::distance(m_logical_line.begin(), m_logical_line.end()))),
                semantics::adjusting_state::MACRO_REPARSE);

            const auto adj_range = [&rp](
                                       size_t start_lineno, size_t start_column, size_t end_lineno, size_t end_column) {
                return rp.adjust_range(
                    range((position(start_lineno, start_column)), (position(end_lineno, end_column))));
            };

            auto [instruction, first_line_skipped, label] = check_line(text, lineno, adj_range);
            if (instruction.first == line_type::ignore)
            {
                it = backup_it;
                m_result.emplace_back(*it++);
                skip_continuation = is_continued(text);
                continue;
            }

            m_source_translated = true;

            m_preproc_details = {};
            m_args = {};

            instruction.first = process_nonempty_line(
                lineno, include_allowed, instruction.first, first_line_skipped, label.name, adj_range);

            m_preproc_details.stmt_r = adj_range(lineno, 0, lineno, text.length());
            m_preproc_details.label = std::move(label);
            m_preproc_details.instruction = std::move(instruction.second);

            if (!m_preproc_details.operands.items.empty())
                m_preproc_details.operands.overall_r = range(
                    m_preproc_details.operands.items.front().r.start, m_preproc_details.operands.items.back().r.end);
            if (!m_preproc_details.remarks.items.empty())
                m_preproc_details.remarks.overall_r =
                    range(m_preproc_details.remarks.items.front().start, m_preproc_details.remarks.items.back().end);

            auto stmt = std::make_shared<semantics::preprocessor_statement_si>(
                std::move(m_preproc_details), instruction.first == line_type::include);

            do_highlighting(*stmt, m_src_proc);

            m_preproc_details.operands.items = std::move(m_args);
            set_statement(std::move(stmt));
        }
    }

    // Inherited via preprocessor
    document generate_replacement(document doc) override
    {
        reset();
        m_source_translated = false;
        m_result.clear();
        m_result.reserve(doc.size());

        auto it = doc.begin();
        const auto end = doc.end();

        skip_process(it, end);
        // ignores ICTL
        inject_SQLSECT();

        generate_replacement(it, end, true);

        if (m_source_translated || !m_conditional)
            return document(std::move(m_result));
        else
            return doc;
    }

    void do_highlighting(const semantics::preprocessor_statement_si& stmt,
        semantics::source_info_processor& src_proc,
        size_t continue_column = 15) const override
    {
        preprocessor::do_highlighting(stmt, src_proc, continue_column);

        size_t lineno = stmt.m_details.stmt_r.start.line;
        for (size_t i = 0; i < m_logical_line.segments.size(); ++i)
        {
            const auto& segment = m_logical_line.segments[i];

            if (!segment.continuation.empty())
                m_src_proc.add_hl_symbol(token_info(
                    range(position(lineno + i, 71), position(lineno + i, 72)), semantics::hl_scopes::continuation));

            if (!segment.ignore.empty())
                m_src_proc.add_hl_symbol(
                    token_info(range(position(lineno + i, 72),
                                   position(lineno + i, 72 + segment.ignore.length() - segment.continuation.empty())),
                        semantics::hl_scopes::ignored));
        }
    }

public:
    db2_preprocessor(const db2_preprocessor_options& opts,
        library_fetcher libs,
        diagnostic_op_consumer* diags,
        semantics::source_info_processor& src_proc)
        : m_version(opts.version)
        , m_conditional(opts.conditional)
        , m_libs(std::move(libs))
        , m_diags(diags)
        , m_src_proc(src_proc)
    {}
};
} // namespace

std::unique_ptr<preprocessor> preprocessor::create(const db2_preprocessor_options& opts,
    library_fetcher libs,
    diagnostic_op_consumer* diags,
    semantics::source_info_processor& src_proc)
{
    return std::make_unique<db2_preprocessor>(opts, std::move(libs), diags, src_proc);
}

} // namespace hlasm_plugin::parser_library::processing
