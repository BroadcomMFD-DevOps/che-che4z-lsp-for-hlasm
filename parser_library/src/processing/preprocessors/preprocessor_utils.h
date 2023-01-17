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

#ifndef HLASMPARSER_PARSERLIBRARY_PROCESSING_PREPROCESSOR_UTILS_H
#define HLASMPARSER_PARSERLIBRARY_PROCESSING_PREPROCESSOR_UTILS_H

#include <memory>
#include <optional>
#include <regex>
#include <string_view>
#include <vector>

#include "semantics/range_provider.h"
#include "semantics/statement.h"

namespace hlasm_plugin::parser_library::processing {

template<typename ITERATOR>
struct stmt_part_details
{
    struct it_pair
    {
        ITERATOR s;
        ITERATOR e;
    };

    it_pair stmt;
    std::optional<it_pair> label;
    it_pair instruction;
    std::optional<std::string> preferred_instruction_name;
    it_pair operands;
    std::optional<it_pair> remarks;

    bool copy_like;
};

// This function fills an operand list with their ranges while expecting to receive a string_view of a single line
// where operands are separated by spaces or commas
void fill_operands_list(std::string_view operands,
    size_t op_column_start,
    const semantics::range_provider& rp,
    std::vector<semantics::preproc_details::name_range>& operand_list);

template<typename ITERATOR>
std::shared_ptr<semantics::preprocessor_statement_si> get_preproc_statement(
    const stmt_part_details<ITERATOR>& iterators, size_t lineno, size_t continue_column = 15);

template<typename It>
static std::true_type same_line_detector(const It& t, decltype(t.same_line(t)) = false);
static std::false_type same_line_detector(...);

template<typename It>
static bool same_line(const It& l, const It& r)
{
    if constexpr (decltype(same_line_detector(l))::value)
        return l.same_line(r);
    else
        return true;
}

template<typename It>
using separator_function = std::function<size_t(const It& it, const It& it_e)>;

template<typename It>
static constexpr const auto space_separator =
    [](const It& it, const It& it_e) { return (it == it_e || *it != ' ') ? 0 : 1; };

template<typename It>
static constexpr const auto no_separator = [](const It&, const It&) { return 0; };

template<typename It>
static void trim_left(It& it, const It& it_e, const separator_function<It>& is_separator)
{
    while (it != it_e)
    {
        auto sep_size = is_separator(it, it_e);
        if (sep_size == 0)
            break;
        std::advance(it, sep_size);
    }
}

template<typename It>
static bool skip_past_next_continuous_sequence(It& it, const It& it_e, const separator_function<It>& is_separator)
{
    It seq_start = it;
    while (it != it_e && !is_separator(it, it_e))
        it++;

    return it != seq_start;
}

template<typename It>
static std::optional<std::string> next_continuous_sequence(
    It& it, const It& it_e, const separator_function<It>& is_separator)
{
    It seq_start = it;
    return !skip_past_next_continuous_sequence(it, it_e, is_separator)
        ? std::nullopt
        : std::optional<std::string>(std::string(seq_start, it));
}

struct words_to_consume
{
    bool needs_same_line;
    bool tolerate_no_space_at_end;
    const std::vector<std::string> words_uc;
    const std::vector<std::string> words_lc;

    words_to_consume(
        const std::vector<std::string>& words, bool needs_same_line, bool tolerate_no_space_at_end) noexcept
        : needs_same_line(needs_same_line)
        , tolerate_no_space_at_end(tolerate_no_space_at_end)
        , words_uc(case_transform(words, toupper))
        , words_lc(case_transform(words, tolower))
    {
        assert(words_uc.size() == words_lc.size());
        assert(std::equal(words_uc.begin(), words_uc.end(), words_lc.begin(), [](const auto& w_uc, const auto& w_lc) {
            return w_uc.length() == w_lc.length();
        }));
    }

private:
    static std::vector<std::string> case_transform(std::vector<std::string> words, const auto& f)
    {
        std::transform(words.begin(), words.end(), words.begin(), [&f](std::string& w) {
            std::transform(w.begin(), w.end(), w.begin(), [&f](unsigned char c) { return static_cast<char>(f(c)); });
            return w;
        });

        return words;
    }
};

template<typename It>
static std::optional<It> consume_words_advance_to_next(
    It& it, const It& it_e, const words_to_consume& wtc, const separator_function<It>& is_separator)
{
    std::optional<It> consumed_word_end = std::nullopt;

    const auto reverter = [backup = it, &it]() {
        it = backup;
        return std::nullopt;
    };

    for (size_t i = 0; i < wtc.words_uc.size(); ++i)
    {
        const auto& w_uc = wtc.words_uc[i];
        const auto& w_lc = wtc.words_lc[i];

        if (consumed_word_end && *consumed_word_end == it)
            return reverter();

        It consumed_word_start = it;
        for (size_t j = 0; j < w_uc.length(); ++j)
        {
            const auto& c_uc = w_uc[j];
            const auto& c_lc = w_lc[j];

            if (it == it_e || (*it != c_uc && *it != c_lc))
                return reverter();

            it++;
        }

        if (wtc.needs_same_line && !same_line(consumed_word_start, std::prev(it)))
            return reverter();

        consumed_word_end = it;
        trim_left(it, it_e, is_separator);
    }

    if (!wtc.tolerate_no_space_at_end && consumed_word_end && *consumed_word_end == it)
        return reverter();

    return consumed_word_end;
}

template<typename It>
static std::optional<It> consume_words(It& it, const It& it_e, const words_to_consume& wtc)
{
    return consume_words_advance_to_next<It>(it, it_e, wtc, no_separator<It>);
}

} // namespace hlasm_plugin::parser_library::processing

#endif