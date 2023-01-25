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

#ifndef HLASMPLUGIN_HLASMPARSERLIBRARY_LOGICAL_LINE_H
#define HLASMPLUGIN_HLASMPARSERLIBRARY_LOGICAL_LINE_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "utils/unicode_text.h"

namespace hlasm_plugin::parser_library::lexing {

// termination character of a line in a file
enum class logical_line_segment_eol : uint8_t
{
    none,
    lf,
    crlf,
    cr,
};

// HLASM logical line/statement representation
// segment 1: <code..............................><continuation><ignore...><logical_line_segment_eol>
// segment 2:              <code.................><continuation><ignore...><logical_line_segment_eol>
// segment 3:              <code.................><ignore.................><logical_line_segment_eol>

// a single line in a file that is a part of the logical (continued) HLASM line/statement.
template<typename It>
struct logical_line_segment
{
    It begin;
    It code;
    It continuation;
    It ignore;
    It end;

    bool continuation_error;
    bool so_si_continuation;

    logical_line_segment_eol eol;
};

template<typename It>
size_t logical_distance(It b, It e)
{
    return std::distance(b, e);
}

template<typename It>
requires requires(It i) { i.counter()->size_t; }
size_t logical_distance(It b, It e) { return e.counter() - b.counter(); }

// represents a single (possibly continued) HLASM line/statement
template<typename It>
struct logical_line
{
    std::vector<logical_line_segment<It>> segments;
    bool continuation_error;
    bool so_si_continuation;
    bool missing_next_line;

    void clear() noexcept
    {
        segments.clear();
        continuation_error = false;
        so_si_continuation = false;
        missing_next_line = false;
    }

    struct const_iterator
    {
        using segment_iterator = typename std::vector<logical_line_segment<It>>::const_iterator;
        using column_iterator = It;

        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = char;
        using pointer = const char*;
        using reference = const char&;

        const_iterator() = default;
        const_iterator(segment_iterator segment_it, column_iterator col_it, const logical_line* ll) noexcept
            : m_segment_it(segment_it)
            , m_col_it(col_it)
            , m_logical_line(ll)
        {}

        reference operator*() const noexcept { return *m_col_it; }
        pointer operator->() const noexcept { return std::to_address(m_col_it); }
        const_iterator& operator++() noexcept
        {
            assert(m_logical_line);
            ++m_col_it;
            while (m_col_it == m_segment_it->continuation)
            {
                if (++m_segment_it == m_logical_line->segments.end())
                {
                    m_col_it = column_iterator();
                    break;
                }
                m_col_it = m_segment_it->code;
            }
            return *this;
        }
        const_iterator operator++(int) noexcept
        {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        const_iterator& operator--() noexcept
        {
            assert(m_logical_line);
            while (m_segment_it == m_logical_line->segments.end() || m_col_it == m_segment_it->code)
            {
                --m_segment_it;
                m_col_it = m_segment_it->continuation;
            }
            --m_col_it;
            return *this;
        }
        const_iterator operator--(int) noexcept
        {
            const_iterator tmp = *this;
            --(*this);
            return tmp;
        }
        friend bool operator==(const const_iterator& a, const const_iterator& b) noexcept
        {
            assert(a.m_logical_line == b.m_logical_line);
            return a.m_segment_it == b.m_segment_it && a.m_col_it == b.m_col_it;
        }
        friend bool operator!=(const const_iterator& a, const const_iterator& b) noexcept { return !(a == b); }

        bool same_line(const const_iterator& o) const noexcept
        {
            assert(m_logical_line == o.m_logical_line);
            return m_segment_it == o.m_segment_it;
        }

        std::pair<size_t, size_t> get_coordinates() const noexcept
        {
            assert(m_logical_line);

            if (m_segment_it == m_logical_line->segments.end())
                return { 0, 0 };

            return { logical_distance(m_segment_it->begin, m_col_it),
                logical_distance(m_logical_line->segments.begin(), m_segment_it) };
        }

    private:
        segment_iterator m_segment_it = segment_iterator();
        column_iterator m_col_it = It();
        const logical_line* m_logical_line = nullptr;
    };

    const_iterator begin() const noexcept
    {
        for (auto s = segments.begin(); s != segments.end(); ++s)
            if (s->code != s->continuation)
                return const_iterator(s, s->code, this);
        return end();
    }
    const_iterator end() const noexcept
    {
        return const_iterator(segments.end(), std::string_view::const_iterator(), this);
    }
};

// defines the layout of the hlasm source file and options to follow for line extraction
struct logical_line_extractor_args
{
    std::size_t begin; // 1-40
    std::size_t end; // 41-80
    std::size_t continuation; // begin+1-40, or 0 if off
    bool dbcs;
    bool eof_copy_rules;
};

constexpr const logical_line_extractor_args default_ictl = { 1, 71, 16, false, false };
constexpr const logical_line_extractor_args default_ictl_dbcs = { 1, 71, 16, true, false };
constexpr const logical_line_extractor_args default_ictl_copy = { 1, 71, 16, false, true };
constexpr const logical_line_extractor_args default_ictl_dbcs_copy = { 1, 71, 16, true, true };

// remove and return a single line from the input (terminated by LF, CRLF, CR, EOF)
std::pair<std::string_view, logical_line_segment_eol> extract_line(std::string_view& input);

template<typename It, typename Sentinel>
std::pair<std::pair<It, It>, logical_line_segment_eol> extract_line(It& input, const Sentinel& s)
{
    auto start = input;
    typename std::iterator_traits<It>::value_type c {};
    while (input != s)
    {
        c = *input;
        if (c == '\r' || c == '\n')
            break;
        ++input;
    }
    auto end = input;
    if (input == s)
        return std::make_pair(std::make_pair(start, end), logical_line_segment_eol::none);
    ++input;
    if (c == '\n')
        return std::make_pair(std::make_pair(start, end), logical_line_segment_eol::lf);
    if (input == s || *input != '\n')
        return std::make_pair(std::make_pair(start, end), logical_line_segment_eol::cr);
    ++input;

    return std::make_pair(std::make_pair(start, end), logical_line_segment_eol::crlf);
}

// appends a logical line segment to the logical line extracted from the input
// returns "need more" (appended line was continued), input must be non-empty
template<typename It, typename Sentinel>
bool append_to_logical_line(
    logical_line<std::remove_cvref_t<It>>& out, It&& input, const Sentinel& s, const logical_line_extractor_args& opts)
{
    auto [line_its, eol] = extract_line(input, s);

    auto& segment = out.segments.emplace_back();

    auto& it = line_its.first;
    const auto& end = line_its.second;

    segment.begin = it;
    utils::utf8_next(it, opts.begin - 1, end);
    segment.code = it;
    utils::utf8_next(it, opts.end + 1 - opts.begin, end);
    segment.continuation = it;
    utils::utf8_next(it, 1, end);
    segment.ignore = it;
    segment.end = end;
    segment.eol = eol;

    if (segment.continuation == segment.ignore)
        return false;

    if (*segment.continuation == ' ' || opts.end == 80 || opts.continuation == 0)
    {
        segment.ignore = segment.continuation;
        return false;
    }

    // line is continued

    if (opts.dbcs)
    {
        auto extended_cont = std::mismatch(std::make_reverse_iterator(segment.continuation),
            std::make_reverse_iterator(segment.code),
            std::make_reverse_iterator(segment.ignore))
                                 .first.base();

        utils::utf8_next(extended_cont, 0, segment.continuation);

        if (extended_cont != segment.continuation)
        {
            segment.continuation = extended_cont;
            if (const auto c = *segment.continuation; c == '<' || c == '>')
                out.so_si_continuation |= segment.so_si_continuation = true;
        }
    }

    return true;
}

template<typename Range>
std::pair<bool, decltype(std::begin(std::declval<Range&&>()))> append_to_logical_line(
    logical_line<decltype(std::begin(std::declval<Range&&>()))>& out,
    Range&& range,
    const logical_line_extractor_args& opts)
{
    std::pair<bool, decltype(std::begin(std::declval<Range&&>()))> result(false, std::begin(range));
    result.first = append_to_logical_line(out, result.second, std::end(range), opts);
    return result;
}

// logical line post-processing
template<typename It>
void finish_logical_line(logical_line<It>& out, const logical_line_extractor_args& opts)
{
    if (out.segments.empty())
        return;

    const size_t cont_size = opts.continuation - opts.begin;
    for (size_t i = 1; i < out.segments.size(); ++i)
    {
        auto& s = out.segments[i];

        auto blank_start = s.code;
        utils::utf8_next(s.code, cont_size, s.continuation);

        out.continuation_error |= s.continuation_error =
            std::any_of(blank_start, s.code, [](unsigned char c) { return c != ' '; });
    }
    auto& last = out.segments.back();
    if (!opts.eof_copy_rules)
    {
        out.missing_next_line = last.continuation != last.ignore;
    }
    else
        last.ignore = last.continuation;
}

// extract a logical line (extracting lines while continued and not EOF)
// returns true when a logical line was extracted
template<typename It, typename Sentinel>
bool extract_logical_line(
    logical_line<std::remove_cvref_t<It>>& out, It&& input, const Sentinel& s, const logical_line_extractor_args& opts)
{
    out.clear();

    if (input == s)
        return false;

    do
    {
        if (!append_to_logical_line(out, input, s, opts))
            break;
    } while (input != s);

    finish_logical_line(out, opts);

    return true;
}

template<typename Range>
std::pair<bool, decltype(std::begin(std::declval<Range&&>()))> extract_logical_line(
    logical_line<decltype(std::begin(std::declval<Range&&>()))>& out,
    Range&& range,
    const logical_line_extractor_args& opts)
{
    std::pair<bool, decltype(std::begin(std::declval<Range&&>()))> result(false, std::begin(range));
    result.first = extract_logical_line(out, result.second, std::end(range), opts);
    return result;
}

} // namespace hlasm_plugin::parser_library::lexing

#endif // HLASMPLUGIN_HLASMPARSERLIBRARY_LOGICAL_LINE_H