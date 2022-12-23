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
#include <stack>
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
#include "utils/unicode_text.h"
#include "workspaces/parse_lib_provider.h"

namespace hlasm_plugin::parser_library::processing {
namespace {
using utils::concat;

using range_adjuster = std::function<range(
    size_t start_lineno, size_t start_column, size_t end_lineno, size_t end_column)>; // todo check for null

struct db2_logical_line : public lexing::logical_line
{
    void clear() override
    {
        lexing::logical_line::clear();
        m_original_code.clear();
        m_comments.clear();
    }

    size_t distance_from_beginning(const lexing::logical_line::const_iterator& e)
    {
        size_t d = std::distance(begin(), e);

        for (size_t i = 0, total = 0; d > total && i < segments.size(); ++i)
            d += m_comments[i].empty() ? 0 : m_comments[i].length() - 2;

        return d;
    }

    lexing::logical_line::const_iterator begin_with_offset(size_t offset)
    {
        auto it = begin();
        while (offset-- != 0 && it != end())
            it++;

        return it;
    }

    static void iterate_by(
        lexing::logical_line::const_iterator& it, const lexing::logical_line::const_iterator& it_e, size_t offset)
    {
        while (offset > 0 && it != it_e)
        {
            it++;
            --offset;
        }
    }

    static void trim_left(lexing::logical_line::const_iterator& it,
        const lexing::logical_line::const_iterator& it_e) // todo should consider strings with '--'
    {
        while (it != it_e)
        {
            if (auto fw_lookup = it; (*it == ' ') || (*it == '-' && (++fw_lookup != it_e && *fw_lookup == '-')))
                it = fw_lookup;
            else
                break;
            it++;
        }
    }

    std::vector<std::string_view> m_original_code;
    std::vector<std::string_view> m_comments;
};

bool append_to_db2_logical_line(
    db2_logical_line& out, std::string_view& input, const lexing::logical_line_extractor_args& opts)
{
    return lexing::append_to_logical_line(out, input, opts);
}

void finish_db2_logical_line(db2_logical_line& out, const lexing::logical_line_extractor_args& opts)
{
    lexing::finish_logical_line(out, opts);

    std::stack<unsigned char> quotes;
    for (auto& seg : out.segments)
    {
        auto& code = seg.code;
        out.m_original_code.emplace_back(seg.code);
        auto& comment = out.m_comments.emplace_back();

        bool comment_possibly_started = false;
        for (size_t j = 0; j < code.size(); ++j)
        {
            const auto& c = code[j];
            if (c == '\"' || c == '\'')
            {
                if (quotes.empty() || quotes.top() != c)
                    quotes.push(c);
                else if (quotes.top() == c)
                    quotes.pop();

                continue;
            }

            if (quotes.empty() && c == '-')
                if (!comment_possibly_started)
                    comment_possibly_started = true;
                else if (comment_possibly_started)
                {
                    comment = code.substr(j - 1);
                    code = code.substr(0, j + 1);
                    break;
                }
        }
    }
}

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

        for (unsigned char c = '0'; c <= '9'; ++c)
            r[c] = symbol_type::ord_char;
        for (unsigned char c = 'A'; c <= 'Z'; ++c)
            r[c] = symbol_type::ord_char;
        for (unsigned char c = 'a'; c <= 'z'; ++c)
            r[c] = symbol_type::ord_char;

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
        It b, It e, size_t lineno, const range_adjuster& r_adj, db2_logical_line& db2_ll)
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

        const auto op_inserter = [&operands, &lineno, &r_adj, &db2_ll](It start, It end) {
            operands.emplace_back(semantics::preproc_details::name_range(std::string(start, end),
                r_adj(lineno, db2_ll.distance_from_beginning(start), lineno, db2_ll.distance_from_beginning(end))));
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
    db2_logical_line m_original_logical_line;
    db2_logical_line m_logical_line;
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
                if (tolerate_no_space_at_end && (l.empty() || l.starts_with("--")) && &w == words.end() - 1)
                    return consumed_words_spread;
                l = init_l; // all or nothing
                return 0;
            }
        }

        return consumed_words_spread;
    }

    std::optional<semantics::preproc_details::name_range> try_process_include(
        lexing::logical_line::const_iterator it, size_t lineno, const range_adjuster& r_adj)
    {
        static const auto include_pattern = std::regex("(INCLUDE)([ ]|--)*(.*)", std::regex_constants::icase);
        static const auto member_pattern = std::regex("(.*?)(?:[ ]|--|$)+");

        if (std::match_results<lexing::logical_line::const_iterator> inc_match;
            !std::regex_match(it, m_logical_line.end(), inc_match, include_pattern))
            return std::nullopt;
        else
            it = inc_match[2].length() == 0 ? inc_match[1].second : inc_match[2].first;

        lexing::logical_line::const_iterator inc_it_s, inc_it_e;
        semantics::preproc_details::name_range nr;

        for (auto reg_it = std::regex_iterator<lexing::logical_line::const_iterator>(
                      it, m_logical_line.end(), member_pattern),
                  reg_it_e = std::regex_iterator<lexing::logical_line::const_iterator>();
             reg_it != reg_it_e;
             ++reg_it)
        {
            if (const auto& sub_match = (*reg_it)[1]; sub_match.length())
            {
                if (nr.name.empty())
                    inc_it_s = sub_match.first;
                inc_it_e = sub_match.second;

                if (!nr.name.empty())
                    nr.name.push_back(' ');
                nr.name.append(sub_match.str());
            }
        }

        if (!nr.name.empty())
            nr.r = r_adj(lineno,
                m_logical_line.distance_from_beginning(inc_it_s),
                lineno,
                m_logical_line.distance_from_beginning(inc_it_e));

        return nr;
    }

    static bool is_end(std::string_view s)
    {
        if (!utils::consume(s, "END"))
            return false;

        if (s.empty() || s.front() == ' ')
            return true;

        return false;
    }

    bool skip_past_is(lexing::logical_line::const_iterator& it_s)
    {
        auto it_i = it_s++;
        if (it_i == m_logical_line.end() || *it_i != 'I')
            return false;

        if (it_s == m_logical_line.end() || *it_s != 'S')
            return false;

        if (!it_s.same_line(it_i))
            return false;

        db2_logical_line::trim_left(++it_s, m_logical_line.end());
        return true;
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
            if (auto consumed_size = consume_words_and_trim(line_preview, words_to_consume, true))
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
                return consume_and_create(line_preview, line_type::sql_type, { "SQL", "TYPE" }, "SQL TYPE");

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

    bool handle_lob(const std::regex& pattern, std::string_view label, lexing::logical_line::const_iterator& it)
    {
        std::match_results<lexing::logical_line::const_iterator> match;
        if (!std::regex_match(it, m_logical_line.end(), match, pattern))
            return false;

        auto s = (match[4].matched ? match[4] : match[1]).str();
        switch (s.back())
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

    bool process_sql_type_operands(
        std::string_view label, lexing::logical_line::const_iterator it, size_t lineno, const range_adjuster& r_adj)
    {
        if (it == m_logical_line.end())
            return false;

        // keep the capture groups in sync
        static const auto xml_type =
            std::regex("XML(?:[ ]|--)+AS(?:[ ]|--)+(?:(BINARY(?:[ ]|--)+LARGE(?:[ ]|--)+OBJECT|BLOB|CHARACTER(?:[ "
                       "]|--)+LARGE(?:[ ]|--)+OBJECT|CHAR(?:[ ]|--)+LARGE(?:[ ]|--)+OBJECT|CLOB|DBCLOB)(?:[ "
                       "]|--)+([[:digit:]]{1,9})([KMG])?|(BLOB_FILE|CLOB_FILE|DBCLOB_FILE))(?: .*)?");
        static const auto lob_type =
            std::regex("(?:(BINARY(?:[ ]|--)+LARGE(?:[ ]|--)+OBJECT|BLOB|CHARACTER(?:[ ]|--)+LARGE(?:[ "
                       "]|--)+OBJECT|CHAR(?:[ ]|--)+LARGE(?:[ ]|--)+OBJECT|CLOB|DBCLOB)(?:[ "
                       "]|--)+([[:digit:]]{1,9})([KMG])?|(BLOB_FILE|CLOB_FILE|DBCLOB_FILE|BLOB_LOCATOR|CLOB_LOCATOR|"
                       "DBCLOB_LOCATOR))(?: .*)?");

        static const auto table_like = std::regex(
            "TABLE(?:[ ]|--)+LIKE(?:[ ]|--)+('(?:[^']|'')+'|(?:[^']|'')+)(?:[ ]|--)+AS(?:[ ]|--)+LOCATOR(?: .*)?");

        static const auto result_set_locator_type =
            std::regex("SULT_SET_LOCATOR(?:[ ]|--)+VARYING(?: .*)?"); // todo tolerate_no_space_at_end
        static const auto row_id_type = std::regex("WID(?: .*)?");

        switch (*it)
        {
            case 'R':
                if (it++; it == m_logical_line.end())
                    return false;

                switch (*it)
                {
                    case 'E':
                        if (it++; !std::regex_match(it, m_logical_line.end(), result_set_locator_type))
                            break;
                        add_ds_line(label, "", "FL4");
                        return true;

                    case 'O':
                        if (it++; !std::regex_match(it, m_logical_line.end(), row_id_type))
                            break;
                        add_ds_line(label, "", "H,CL40");
                        return true;
                }
                break;

            case 'T':
                if (!std::regex_match(it, m_logical_line.end(), table_like))
                    break;
                add_ds_line(label, "", "FL4");
                return true;

            case 'X':
                return handle_lob(xml_type, label, it);

            case 'B':
            case 'C':
            case 'D':
                return handle_lob(lob_type, label, it);
        }
        return false;
    }

    void process_regular_line(
        size_t lineno, std::string_view label, size_t first_line_skipped, const range_adjuster& r_adj)
    {
        if (!label.empty())
            m_result.emplace_back(replaced_line { concat(label, " DS 0H\n") });

        m_result.emplace_back(replaced_line { "***$$$\n" });

        for (const auto& segment : m_logical_line.segments)
        {
            std::string this_line(segment.line);

            if (first_line_skipped)
            {
                first_line_skipped = 0;
                const auto appended_line_size = segment.line.size();
                if (!label.empty())
                    this_line.replace(this_line.size() - appended_line_size,
                        label.size(),
                        label.size(),
                        ' '); // mask out any label-like characters
                this_line[this_line.size() - appended_line_size] = '*';
            }

            this_line.append("\n");
            m_result.emplace_back(replaced_line { std::move(this_line) });
        }
    }

    void process_sql_type_line(size_t lineno, size_t first_line_skipped, const range_adjuster& r_adj)
    {
        m_result.emplace_back(replaced_line { "***$$$\n" });
        m_result.emplace_back(replaced_line {
            concat("*", m_logical_line.m_original_code.front().substr(0, lexing::default_ictl.end - 1), "\n") });
        m_result.emplace_back(replaced_line { "***$$$\n" });
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

    std::vector<semantics::preproc_details::name_range> process_nonempty_line(size_t lineno,
        bool include_allowed,
        std::pair<line_type, semantics::preproc_details::name_range>& instruction_info,
        size_t first_line_skipped,
        std::string_view label,
        const range_adjuster& r_adj)
    {
        if (m_logical_line.continuation_error && m_diags)
            m_diags->add_diagnostic(diagnostic_op::error_DB001(range(position(lineno, 0))));

        std::vector<semantics::preproc_details::name_range> args;
        auto& [instr_type, instr_name_range] = instruction_info;
        auto it = m_logical_line.begin_with_offset(instr_name_range.r.end.column);
        db2_logical_line::trim_left(it, m_logical_line.end());

        switch (instr_type)
        {
            case line_type::exec_sql: {
                process_regular_line(lineno, label, first_line_skipped, r_adj);
                if (auto inc_member_details = try_process_include(it, lineno, r_adj); inc_member_details.has_value())
                {
                    if (inc_member_details->name.empty())
                    {
                        if (m_diags)
                            m_diags->add_diagnostic(diagnostic_op::warn_DB007(range(position(lineno, 0))));
                        break;
                    }

                    instr_type = line_type::include;
                    args.emplace_back(std::move(*inc_member_details));
                    if (include_allowed)
                        process_include(args.back().name, lineno);
                    else if (m_diags)
                        m_diags->add_diagnostic(
                            diagnostic_op::error_DB003(range(position(lineno, 0)), args.back().name));
                }
                else
                {
                    if (sql_has_codegen(it))
                    {
                        mini_parser<lexing::logical_line::const_iterator> p;
                        args = p.parse_and_substitute(it, m_logical_line.end(), lineno, r_adj, m_logical_line);
                        generate_sql_code_mock(args.size());
                    }
                    m_result.emplace_back(replaced_line { "***$$$\n" });
                }

                break;
            }
            case line_type::sql_type:
                process_sql_type_line(lineno, first_line_skipped, r_adj);
                // DB2 preprocessor exhibits strange behavior when SQL TYPE line is continued
                if (m_logical_line.segments.size() > 1 && m_diags)
                    m_diags->add_diagnostic(diagnostic_op::warn_DB005(range(position(lineno, 0))));
                if (!skip_past_is(it))
                {
                    if (m_diags)
                        m_diags->add_diagnostic(diagnostic_op::warn_DB006(range(position(lineno, 0))));
                    break;
                }
                if (label.empty())
                    label = " "; // best matches the observed behavior
                if (!process_sql_type_operands(label, it, lineno, r_adj) && m_diags)
                    m_diags->add_diagnostic(diagnostic_op::error_DB004(range(position(lineno, 0))));
                break;
        }

        return args;
    }

    bool sql_has_codegen(lexing::logical_line::const_iterator it)
    {
        // handles only the most obvious cases (imprecisely)
        static const auto no_code_statements =
            std::regex("(?:DECLARE|WHENEVER|BEGIN(?:[ ]|--)+DECLARE(?:[ ]|--)+SECTION|END(?:[ ]|--)+DECLARE(?:[ "
                       "]|--)+SECTION)(?: .*)?",
                std::regex_constants::icase);
        return !std::regex_match(it, m_logical_line.end(), no_code_statements);
    }
    void generate_sql_code_mock(size_t in_params)
    {
        // this function generates semi-realistic sql statement replacement code, because people do strange
        // things... <arguments> output parameters
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

            it = extract_nonempty_db2_logical_line(m_logical_line, it, end, lexing::default_ictl);
            if (include_allowed)
                m_original_logical_line = m_logical_line;

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

            semantics::preproc_details preproc_details {
                adj_range(lineno, 0, lineno, text.length()), std::move(label), std::move(instruction.second)
            };
            auto ops = process_nonempty_line(
                lineno, include_allowed, instruction, first_line_skipped, preproc_details.label.name, adj_range);

            auto stmt = std::make_shared<semantics::preprocessor_statement_si>(
                std::move(preproc_details), instruction.first == line_type::include);

            if (include_allowed)
                m_logical_line = m_original_logical_line;
            do_highlighting(*stmt, m_logical_line, m_src_proc);

            stmt->m_details.operands = std::move(ops);
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
        const lexing::logical_line& ll,
        semantics::source_info_processor& src_proc,
        size_t continue_column = 15) const override
    {
        preprocessor::do_highlighting(stmt, ll, src_proc, continue_column);

        auto db2_ll = dynamic_cast<const db2_logical_line&>(ll);
        for (size_t i = 0, lineno = stmt.m_details.stmt_r.start.line; i < db2_ll.segments.size(); ++i, ++lineno)
        {
            const auto& segment = db2_ll.segments[i];

            size_t code_len = 0;
            if (const auto& code = segment.code; !code.empty())
            {
                code_len = code.length();
                auto comment_offset = db2_ll.m_comments[i].empty() ? 0 : 2;

                size_t start_column = i == 0 ? stmt.m_details.instruction.r.end.column : continue_column;
                src_proc.add_hl_symbol(
                    token_info(range(position(lineno, start_column),
                                   position(lineno, start_column * !!i + code.length() - comment_offset)),
                        semantics::hl_scopes::operand));
            }

            if (const auto& comment = db2_ll.m_comments[i]; !comment.empty())
            {
                src_proc.add_hl_symbol(
                    token_info(range(position(lineno, !!i * continue_column + code_len - 2),
                                   position(lineno, !!i * continue_column + code_len - 2 + comment.length())),
                        semantics::hl_scopes::remark));
            }
        }
    }

    line_iterator extract_nonempty_db2_logical_line(
        db2_logical_line& out, line_iterator it, line_iterator end, const lexing::logical_line_extractor_args& opts)
    {
        out.clear();

        while (it != end)
        {
            auto text = it++->text();
            if (!append_to_db2_logical_line(out, text, opts))
                break;
        }

        finish_db2_logical_line(out, opts);

        return it;
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
