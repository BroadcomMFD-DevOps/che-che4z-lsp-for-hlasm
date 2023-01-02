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

struct db2_logical_line final : public lexing::logical_line
{
    void clear() noexcept override
    {
        lexing::logical_line::clear();
        m_first_line = {};
        m_instruction_end = 0;
        m_comments.clear();
    }

    size_t distance(
        const lexing::logical_line::const_iterator& b, const lexing::logical_line::const_iterator& e) const noexcept
    {
        assert(segments.size() == m_comments.size());

        size_t d = std::distance(b, e);

        lexing::logical_line::const_iterator::segment_iterator segment_it;
        for (segment_it = segments.begin(); segment_it != segments.end(); ++segment_it)
            if (b.same_segment(segment_it))
                break;

        for (size_t i = 0; segment_it != segments.end() && !e.same_segment(segment_it); ++segment_it, ++i)
            d += m_comments[i].empty()
                ? 0
                : m_comments[i].length() - 2; // Compensate for both code and comment having the '--' separator

        return d;
    }

    lexing::logical_line::const_iterator begin_from_operands() const noexcept
    {
        auto it = begin();
        iterate_by(it, m_instruction_end);
        trim_left(it);
        return it;
    }

    std::string_view m_first_line;
    size_t m_instruction_end;
    std::vector<std::string_view> m_comments;

private:
    void iterate_by(lexing::logical_line::const_iterator& it, size_t offset) const noexcept
    {
        while (offset > 0 && it != end())
        {
            it = std::next(it);
            --offset;
        }
    }

    void trim_left(lexing::logical_line::const_iterator& it) const noexcept
    {
        while (it != end())
        {
            if (*it == ' ')
                it = std::next(it);
            else if (auto it_n = std::next(it); *it == '-' && (it_n != end() && *it_n == '-'))
                it = std::next(it_n);
            else
                break;
        }
    }
};

enum class symbol_type : unsigned char
{
    other_char,
    ord_char,
    blank,
    colon,
    quote,
    remark_start,
};

constexpr std::array symbols = []() {
    std::array<symbol_type, std::numeric_limits<unsigned char>::max() + 1> r {};

    using enum symbol_type;

    for (unsigned char c = '0'; c <= '9'; ++c)
        r[c] = ord_char;
    for (unsigned char c = 'A'; c <= 'Z'; ++c)
        r[c] = ord_char;
    for (unsigned char c = 'a'; c <= 'z'; ++c)
        r[c] = ord_char;

    r[(unsigned char)'_'] = ord_char;
    r[(unsigned char)'@'] = ord_char;
    r[(unsigned char)'$'] = ord_char;
    r[(unsigned char)'#'] = ord_char;
    r[(unsigned char)' '] = blank;
    r[(unsigned char)':'] = colon;
    r[(unsigned char)'\''] = quote;
    r[(unsigned char)'\"'] = quote;
    r[(unsigned char)'-'] = remark_start;

    return r;
}();

size_t find_start_of_line_comment(
    std::stack<unsigned char, std::basic_string<unsigned char>>& quotes, const std::string_view& code)
{
    bool comment_possibly_started = false;
    size_t comment_start = 0;
    for (const auto& c : code)
    {
        if (auto s = symbols[static_cast<unsigned char>(c)]; s == symbol_type::quote)
        {
            if (quotes.empty() || quotes.top() != c)
                quotes.push(c);
            else if (quotes.top() == c)
                quotes.pop();

            comment_possibly_started = false;
        }
        else if (quotes.empty() && s == symbol_type::remark_start)
        {
            if (!comment_possibly_started)
                comment_possibly_started = true;
            else
                break;
        }

        comment_start++;
    }

    return comment_start;
}

void finish_db2_logical_line(
    db2_logical_line& out, const lexing::logical_line_extractor_args& opts, size_t instruction_end)
{
    lexing::finish_logical_line(out, opts);

    if (out.segments.empty())
        return;

    out.m_first_line = out.segments.front().code;
    out.m_instruction_end = instruction_end;

    std::stack<unsigned char, std::basic_string<unsigned char>> quotes;
    for (auto& seg : out.segments)
    {
        auto& code = seg.code;
        auto& comment = out.m_comments.emplace_back();

        // Code and comment both contain the '--' separator
        if (auto comment_start = find_start_of_line_comment(quotes, code); comment_start != code.length())
        {
            comment = code.substr(comment_start - 1);
            code = code.substr(0, comment_start + 1);
        }
    }
}

using range_adjuster = std::function<range(
    const db2_logical_line::const_iterator& start_column, const db2_logical_line::const_iterator& end_column)>;

template<typename It>
class mini_parser
{
    void skip_to_string_end(It& b, const It& e) const
    {
        if (b == e)
            return;

        const unsigned char quote = *b;
        while (++b != e)
            if (quote == *b)
                break;
    }

public:
    std::vector<semantics::preproc_details::name_range> get_args(It& b, const It& e, const range_adjuster& r_adj)
    {
        assert(r_adj);

        enum class consuming_state
        {
            NON_CONSUMING,
            PREPARE_TO_CONSUME,
            CONSUMING,
            TRAIL,
            QUOTE,
        };

        std::vector<semantics::preproc_details::name_range> arguments;
        const auto try_arg_inserter = [&arguments, &r_adj](const It& start, const It& end, consuming_state state) {
            if (state != consuming_state::CONSUMING)
                return false;

            arguments.emplace_back(
                semantics::preproc_details::name_range { std::string(start, end), r_adj(start, end) });
            return true;
        };

        It arg_start_it;
        consuming_state next_state = consuming_state::NON_CONSUMING;
        while (b != e)
        {
            const auto state = std::exchange(next_state, consuming_state::NON_CONSUMING);

            switch (symbols[static_cast<unsigned char>(*b)])
            {
                using enum symbol_type;
                case ord_char:
                    if (state == consuming_state::PREPARE_TO_CONSUME)
                    {
                        arg_start_it = b;
                        next_state = consuming_state::CONSUMING;
                    }
                    else if (state == consuming_state::CONSUMING)
                        next_state = state;

                    break;

                case colon:
                    if (state == consuming_state::PREPARE_TO_CONSUME || state == consuming_state::TRAIL)
                        break;

                    if (!try_arg_inserter(arg_start_it, b, state))
                        next_state = consuming_state::PREPARE_TO_CONSUME;

                    break;

                case blank:
                    if (try_arg_inserter(arg_start_it, b, state))
                        next_state = consuming_state::TRAIL;
                    else
                        next_state = state;

                    break;

                case quote:
                    try_arg_inserter(arg_start_it, b, state);

                    if (skip_to_string_end(b, e); b == e)
                        goto done;

                    break;

                case remark_start:
                    if (auto n = std::next(b); !try_arg_inserter(arg_start_it, b, state) && n != e
                        && symbols[static_cast<unsigned char>(*n)] == remark_start)
                    {
                        b = n;
                        next_state = state;
                    }

                    break;

                case other_char:
                    try_arg_inserter(arg_start_it, b, state);
                    break;

                default:
                    assert(false);
                    break;
            }

            ++b;
        }

        try_arg_inserter(arg_start_it, b, next_state);

    done:
        return arguments;
    }
};

class db2_preprocessor final : public preprocessor // TODO Take DBCS into account
{
    std::string m_version;
    bool m_conditional;
    library_fetcher m_libs;
    diagnostic_op_consumer* m_diags = nullptr;
    std::vector<document_line> m_result;
    bool m_source_translated = false;
    semantics::source_info_processor& m_src_proc;

    enum class line_type
    {
        ignore,
        exec_sql,
        include,
        sql_type
    };

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

    static std::regex get_consuming_regex(std::initializer_list<std::string_view> words, bool tolerate_no_space_at_end)
    {
        assert(words.size());

        auto w_it = words.begin();

        std::string s = "(";
        s.append(*w_it++);
        while (w_it != words.end())
            s.append("(?:[ ]|--)+(?:").append(*w_it++).append(")");

        s.append(")([ ]|--)");
        if (tolerate_no_space_at_end)
            s.append("*");
        else
            s.append("+");
        s.append("(.*)");

        return std::regex(s);
    }

    template<typename It>
    static std::optional<It> consume_words_advance_to_next(It& it,
        const It& it_e,
        std::initializer_list<std::string_view> words,
        bool needs_same_line,
        bool tolerate_no_space_at_end)
    {
        if (!words.size())
            return std::nullopt;

        if (std::match_results<It> matches;
            std::regex_match(it, it_e, matches, get_consuming_regex(words, tolerate_no_space_at_end))
            && (!needs_same_line || same_line(matches[1].first, std::prev(matches[1].second)))
            && (!tolerate_no_space_at_end || matches[2].length() || !matches[3].length()
                || (matches[1].second == matches[3].first
                    && !same_line(std::prev(matches[1].second), matches[3].first))))
        {
            it = matches[3].first;
            return matches[1].second;
        }

        return std::nullopt;
    }

    std::optional<semantics::preproc_details::name_range> try_process_include(lexing::logical_line::const_iterator it,
        const lexing::logical_line::const_iterator& it_e,
        const range_adjuster& r_adj)
    {
        if (!consume_words_advance_to_next(it, it_e, { "INCLUDE" }, false, false))
            return std::nullopt;

        lexing::logical_line::const_iterator inc_it_s;
        lexing::logical_line::const_iterator inc_it_e;
        semantics::preproc_details::name_range nr;
        static const auto member_pattern = std::regex("(.*?)(?:[ ]|--)*$");

        for (auto reg_it = std::regex_iterator<lexing::logical_line::const_iterator>(it, it_e, member_pattern),
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

        if (!nr.name.empty() && r_adj)
            nr.r = r_adj(inc_it_s, inc_it_e);

        return nr;
    }

    std::pair<line_type, std::string> process_include_member(
        line_type instruction_type, std::string member, size_t lineno)
    {
        auto member_upper = context::to_upper_copy(member);

        if (member_upper == "SQLCA")
        {
            inject_SQLCA();
            return { instruction_type, member_upper };
        }
        if (member_upper == "SQLDA")
        {
            inject_SQLDA();
            return { instruction_type, member_upper };
        }
        m_result.emplace_back(replaced_line { "***$$$\n" });

        std::optional<std::pair<std::string, utils::resource::resource_location>> include_member;
        if (m_libs)
            include_member = m_libs(member_upper);
        if (!include_member.has_value())
        {
            if (m_diags)
                m_diags->add_diagnostic(diagnostic_op::error_DB002(range(position(lineno, 0)), member));
            return { instruction_type, member };
        }

        auto& [include_mem_text, include_mem_loc] = *include_member;
        document d(include_mem_text);
        d.convert_to_replaced();
        generate_replacement(d.begin(), d.end(), false);
        append_included_member(std::make_unique<included_member_details>(included_member_details {
            std::move(member_upper), std::move(include_mem_text), std::move(include_mem_loc) }));
        return { line_type::include, member };
    }

    static bool is_end(std::string_view s) { return utils::consume(s, "END") && (s.empty() || s.front() == ' '); }

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

    static semantics::preproc_details::name_range extract_label(std::string_view& s, size_t lineno)
    {
        auto label = utils::next_nonblank_sequence(s);
        if (!label.length())
            return {};

        s.remove_prefix(label.length());

        return semantics::preproc_details::name_range { std::string(label),
            range((position(lineno, 0)), (position(lineno, label.length()))) };
    }

    static std::pair<line_type, semantics::preproc_details::name_range> extract_instruction(
        const std::string_view& line_preview, size_t lineno, size_t instr_column_start)
    {
        static const std::pair<line_type, semantics::preproc_details::name_range> ignore(line_type::ignore, {});

        if (line_preview.empty())
            return ignore;

        const auto consume_and_create = [&line_preview, lineno, instr_column_start](line_type line,
                                            std::initializer_list<std::string_view> words_to_consume,
                                            std::string_view line_id) {
            auto it = line_preview.begin();
            if (auto consumed_words_end =
                    consume_words_advance_to_next(it, line_preview.end(), words_to_consume, true, false);
                consumed_words_end)
                return std::make_pair(line,
                    semantics::preproc_details::name_range { std::string(line_id),
                        range((position(lineno, instr_column_start)),
                            (position(lineno,
                                instr_column_start + std::distance(line_preview.begin(), *consumed_words_end)))) });
            return ignore;
        };

        switch (line_preview.front())
        {
            case 'E':
                return consume_and_create(line_type::exec_sql, { "EXEC", "SQL" }, "EXEC SQL");

            case 'S':
                return consume_and_create(line_type::sql_type, { "SQL", "TYPE" }, "SQL TYPE");

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

    bool handle_lob(const std::regex& pattern,
        std::string_view label,
        const lexing::logical_line::const_iterator& it,
        const lexing::logical_line::const_iterator& it_e)
    {
        std::match_results<lexing::logical_line::const_iterator> match;
        if (!std::regex_match(it, it_e, match, pattern))
            return false;

        switch (*std::prev((match[4].matched ? match[4] : match[1]).second))
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
                auto len = std::stoll(match[2].str()) * li.scale;

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

    bool handle_r_starting_operands(const std::string_view& label,
        const lexing::logical_line::const_iterator& it_b,
        const lexing::logical_line::const_iterator& it_e)
    {
        auto ds_line_inserter = [&label, &it_e, this](lexing::logical_line::const_iterator it_b,
                                    std::initializer_list<std::string_view> words_to_consume,
                                    std::string_view ds_line_type) {
            if (!consume_words_advance_to_next(it_b, it_e, std::move(words_to_consume), false, true))
                return false;
            add_ds_line(label, "", std::move(ds_line_type));
            return true;
        };

        assert(it_b != it_e && *it_b == 'R');

        if (auto it_n = std::next(it_b); it_n == it_e || (*it_n != 'E' && *it_n != 'O'))
            return false;
        else if (*it_n == 'E')
            return ds_line_inserter(it_b, { "RESULT_SET_LOCATOR", "VARYING" }, "FL4");
        else
            return ds_line_inserter(it_b, { "ROWID" }, "H,CL40");
    };

    bool process_sql_type_operands(const std::string_view& label,
        const lexing::logical_line::const_iterator& it,
        const lexing::logical_line::const_iterator& it_e)
    {
        if (it == it_e)
            return false;

        // keep the capture groups in sync
        static const auto xml_type =
            std::regex("XML(?:[ ]|--)+AS(?:[ ]|--)+"
                       "(?:"
                       "("
                       "BINARY(?:[ ]|--)+LARGE(?:[ ]|--)+OBJECT|BLOB|CHARACTER(?:[ ]|--)+"
                       "LARGE(?:[ ]|--)+OBJECT|CHAR(?:[ ]|--)+LARGE(?:[ ]|--)+OBJECT|CLOB|DBCLOB"
                       ")"
                       "(?:[ ]|--)+([[:digit:]]{1,9})([KMG])?"
                       "|"
                       "(BLOB_FILE|CLOB_FILE|DBCLOB_FILE)"
                       ")"
                       "(?: .*)?");
        static const auto lob_type =
            std::regex("(?:"
                       "("
                       "BINARY(?:[ ]|--)+LARGE(?:[ ]|--)+OBJECT|BLOB|CHARACTER(?:[ ]|--)+"
                       "LARGE(?:[ ]|--)+OBJECT|CHAR(?:[ ]|--)+LARGE(?:[ ]|--)+OBJECT|CLOB|DBCLOB"
                       ")"
                       "(?:[ ]|--)+([[:digit:]]{1,9})([KMG])?"
                       "|"
                       "(BLOB_FILE|CLOB_FILE|DBCLOB_FILE|BLOB_LOCATOR|CLOB_LOCATOR|DBCLOB_LOCATOR)"
                       ")"
                       "(?: .*)?");

        static const auto table_like = std::regex(
            "TABLE(?:[ ]|--)+LIKE(?:[ ]|--)+('(?:[^']|'')+'|(?:[^']|'')+)(?:[ ]|--)+AS(?:[ ]|--)+LOCATOR(?: .*)?");

        switch (*it)
        {
            case 'R':
                return handle_r_starting_operands(label, it, it_e);

            case 'T':
                if (!std::regex_match(it, it_e, table_like))
                    return false;
                add_ds_line(label, "", "FL4");
                return true;

            case 'X':
                return handle_lob(xml_type, label, it, it_e);

            case 'B':
            case 'C':
            case 'D':
                return handle_lob(lob_type, label, it, it_e);
            default:
                return false;
        }
    }

    void process_regular_line(const std::vector<lexing::logical_line_segment>& ll_segments, std::string_view label)
    {
        if (!label.empty())
            m_result.emplace_back(replaced_line { concat(label, " DS 0H\n") });

        m_result.emplace_back(replaced_line { "***$$$\n" });

        bool first_line = true;
        for (const auto& segment : ll_segments)
        {
            std::string this_line(segment.line);

            if (std::exchange(first_line, false))
            {
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

    void process_sql_type_line(const db2_logical_line& ll)
    {
        m_result.emplace_back(replaced_line { "***$$$\n" });
        m_result.emplace_back(
            replaced_line { concat("*", ll.m_first_line.substr(0, lexing::default_ictl.end - 1), "\n") });
        m_result.emplace_back(replaced_line { "***$$$\n" });
    }

    std::tuple<line_type, semantics::preproc_details::name_range, semantics::preproc_details::name_range> check_line(
        std::string_view input, size_t lineno)
    {
        static const std::
            tuple<line_type, semantics::preproc_details::name_range, semantics::preproc_details::name_range>
                ignore(line_type::ignore, {}, {});
        std::string_view line_preview = create_line_preview(input);

        if (ignore_line(line_preview))
            return ignore;

        semantics::preproc_details::name_range label = extract_label(line_preview, lineno);

        auto trimmed = utils::trim_left(line_preview);
        if (!trimmed)
            return ignore;

        if (is_end(line_preview))
        {
            push_sql_working_storage();

            return ignore;
        }

        if (auto [instruction_type, instruction_nr] =
                extract_instruction(line_preview, lineno, label.r.end.column + trimmed);
            instruction_type != line_type::ignore)
            return { std::move(instruction_type), label, std::move(instruction_nr) };

        return ignore;
    }

    std::vector<semantics::preproc_details::name_range> process_nonempty_line(const db2_logical_line& ll,
        size_t lineno,
        bool include_allowed,
        line_type& instruction_type,
        std::string_view label,
        const range_adjuster& r_adj)
    {
        const auto diag_adder = [diags = m_diags](diagnostic_op&& diag) {
            if (diags)
                diags->add_diagnostic(std::move(diag));
        };

        if (ll.continuation_error)
            diag_adder(diagnostic_op::error_DB001(range(position(lineno, 0))));

        std::vector<semantics::preproc_details::name_range> args;
        auto it = ll.begin_from_operands();
        auto it_e = ll.end();

        switch (instruction_type)
        {
            case line_type::exec_sql: {
                process_regular_line(ll.segments, label);
                if (auto inc_member_details = try_process_include(it, it_e, r_adj); inc_member_details.has_value())
                {
                    if (inc_member_details->name.empty())
                    {
                        diag_adder(diagnostic_op::warn_DB007(range(position(lineno, 0))));
                        break;
                    }

                    if (include_allowed)
                        std::tie(instruction_type, inc_member_details->name) =
                            process_include_member(instruction_type, inc_member_details->name, lineno);
                    else
                        diag_adder(diagnostic_op::error_DB003(range(position(lineno, 0)), inc_member_details->name));

                    args.emplace_back(std::move(*inc_member_details));
                }
                else
                {
                    if (sql_has_codegen(it, it_e))
                    {
                        mini_parser<lexing::logical_line::const_iterator> p;
                        args = p.get_args(it, it_e, r_adj);
                        generate_sql_code_mock(args.size());
                    }
                    m_result.emplace_back(replaced_line { "***$$$\n" });
                }

                break;
            }

            case line_type::sql_type:
                process_sql_type_line(ll);
                // DB2 preprocessor exhibits strange behavior when SQL TYPE line is continued
                if (ll.segments.size() > 1)
                    diag_adder(diagnostic_op::warn_DB005(range(position(lineno, 0))));
                if (!consume_words_advance_to_next(it, it_e, { "IS" }, true, true))
                {
                    diag_adder(diagnostic_op::warn_DB006(range(position(lineno, 0))));
                    break;
                }

                if (label.empty())
                    label = " "; // best matches the observed behavior
                if (!process_sql_type_operands(label, it, it_e))
                    diag_adder(diagnostic_op::error_DB004(range(position(lineno, 0))));
                break;

            default:
                break;
        }

        return args;
    }

    bool sql_has_codegen(
        const lexing::logical_line::const_iterator& it, const lexing::logical_line::const_iterator& it_e) const
    {
        // handles only the most obvious cases (imprecisely)
        static const auto no_code_statements = std::regex(
            "(?:DECLARE|WHENEVER|BEGIN(?:[ ]|--)+DECLARE(?:[ ]|--)+SECTION|END(?:[ ]|--)+DECLARE(?:[ ]|--)+SECTION)"
            "(?: .*)?",
            std::regex_constants::icase);
        return !std::regex_match(it, it_e, no_code_statements);
    }

    void generate_sql_code_mock(size_t in_params)
    {
        // this function generates semi-realistic sql statement replacement code, because people do strange things...
        // <arguments> input parameters
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

            auto [instruction_type, label_nr, instruction_nr] = check_line(text, lineno);
            if (instruction_type == line_type::ignore)
            {
                m_result.emplace_back(*it++);
                skip_continuation = is_continued(text);
                continue;
            }

            m_source_translated = true;

            db2_logical_line ll = extract_nonempty_db2_logical_line(instruction_nr.r.end.column, it, end);

            const auto r_adjuster =
                [rp = semantics::range_provider(
                     range(position(lineno, 0), position(lineno, ll.distance(ll.begin(), ll.end()))),
                     semantics::adjusting_state::MACRO_REPARSE),
                    &ll,
                    &lineno](const db2_logical_line::const_iterator& start_column,
                    const db2_logical_line::const_iterator& end_column) {
                    auto dist_from_beginning = ll.distance(ll.begin(), start_column);
                    return rp.adjust_range(range((position(lineno, dist_from_beginning)),
                        (position(lineno, ll.distance(start_column, end_column) + dist_from_beginning))));
                };


            auto args = process_nonempty_line(ll, lineno, include_allowed, instruction_type, label_nr.name, r_adjuster);

            auto stmt = std::make_shared<semantics::preprocessor_statement_si>(
                semantics::preproc_details {
                    r_adjuster(ll.begin(), ll.end()), std::move(label_nr), std::move(instruction_nr) },
                instruction_type == line_type::include);

            do_highlighting(*stmt, ll, m_src_proc);

            stmt->m_details.operands = std::move(args);
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

        const auto& db2_ll = dynamic_cast<const db2_logical_line&>(ll);
        for (size_t i = 0, lineno = stmt.m_details.stmt_r.start.line, line_start_column = 0; i < db2_ll.segments.size();
             ++i, ++lineno, std::exchange(line_start_column, continue_column))
        {
            const auto& code = db2_ll.segments[i].code;
            auto comment_start_column = line_start_column + code.length();

            if (const auto& comment = db2_ll.m_comments[i]; !comment.empty())
            {
                comment_start_column -= 2; // Compensate for both code and comment having the '--' separator
                src_proc.add_hl_symbol(token_info(range(position(lineno, comment_start_column),
                                                      position(lineno, comment_start_column + comment.length())),
                    semantics::hl_scopes::remark));
            }

            if (!code.empty())
                if (auto operand_start_column = i == 0 ? stmt.m_details.instruction.r.end.column : continue_column;
                    operand_start_column < comment_start_column)
                    src_proc.add_hl_symbol(token_info(
                        range(position(lineno, operand_start_column), position(lineno, comment_start_column)),
                        semantics::hl_scopes::operand));
        }
    }

    db2_logical_line extract_nonempty_db2_logical_line(
        size_t instruction_end, line_iterator& it, const line_iterator& end) const
    {
        db2_logical_line out;
        out.clear();

        while (it != end)
        {
            auto text = it++->text();
            if (!append_to_logical_line(out, text, lexing::default_ictl))
                break;
        }

        finish_db2_logical_line(out, lexing::default_ictl, instruction_end);

        return out;
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
