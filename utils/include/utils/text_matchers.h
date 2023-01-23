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

#ifndef HLASMPLUGIN_UTILS_TEXT_MATCHERS_H
#define HLASMPLUGIN_UTILS_TEXT_MATCHERS_H

#include <cctype>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace hlasm_plugin::utils::text_matchers {

template<typename It>
[[maybe_unused]] std::true_type same_line_detector(const It& t, decltype(t.same_line(t)) = false);
[[maybe_unused]] std::false_type same_line_detector(...);

template<typename It>
constexpr bool same_line(const It& l, const It& r)
{
    if constexpr (decltype(same_line_detector(l))::value)
        return l.same_line(r);
    else
        return true;
}

template<bool case_sensitive, bool single_line>
class basic_string_matcher
{
    std::string_view to_match;

public:
    // case_sensitive => to_match assumed in capitals
    constexpr basic_string_matcher(std::string_view to_match)
        : to_match(to_match)
    {}

    template<typename It>
    bool operator()(It& b, const It& e) const noexcept
    {
        It work = b;

        for (auto c : to_match)
        {
            if (work == e)
                return false;
            if constexpr (single_line)
            {
                if (!same_line(work, b))
                    return false;
            }
            auto in_c = *work++;
            if constexpr (!case_sensitive)
                in_c = std::toupper((unsigned char)in_c);

            if (in_c != c)
                return false;
        }
        b = work;
        return true;
    }
};

template<bool negate>
class char_matcher_impl
{
    std::string_view to_match;

public:
    // case_sensitive => to_match assumed in capitals
    constexpr char_matcher_impl(std::string_view to_match)
        : to_match(to_match)
    {}

    template<typename It>
    constexpr bool operator()(It& b, const It& e) const noexcept
    {
        if (b == e)
            return false;
        bool result = (to_match.find(*b) != std::string_view::npos) != negate;
        if (result)
            ++b;
        return result;
    }
};

using char_matcher = char_matcher_impl<false>;
using not_char_matcher = char_matcher_impl<true>;

template<typename Matcher>
constexpr auto star(Matcher&& matcher)
{
    return [matcher = std::forward<Matcher>(matcher)]<typename It>(It& b, const It& e) noexcept {
        while (matcher(b, e))
            ;
        return true;
    };
}
template<typename Matcher>
constexpr auto plus(Matcher&& matcher)
{
    return [matcher = std::forward<Matcher>(matcher)]<typename It>(It& b, const It& e) noexcept {
        bool matched = false;
        while (matcher(b, e))
            matched = true;

        return matched;
    };
}

template<bool empty_allowed, bool single_line>
class space_matcher
{
public:
    template<typename It>
    constexpr bool operator()(It& b, const It& e) const noexcept
    {
        auto work = b;
        while (work != e)
        {
            if constexpr (single_line)
            {
                if (!same_line(work, b))
                    break;
            }
            if (*work != ' ')
                break;
            ++work;
        }
        if constexpr (!empty_allowed)
        {
            if (work == b)
                return false;
        }
        b = work;
        return true;
    }
};

template<typename DefaultStringMatcher = void, typename... Matchers>
constexpr auto seq(Matchers&&... matchers)
{
    constexpr auto convert = []<typename T>(T&& t) {
        if constexpr (std::is_convertible_v<T&&, std::string_view>)
            return DefaultStringMatcher(std::forward<T>(t));
        else
            return std::forward<T>(t);
    };
    return [... matchers = convert(std::forward<Matchers>(matchers))]<typename It>(It& b, const It& e) noexcept {
        auto work = b;
        return ((matchers(work, e) && ...)) && (b = work, true);
    };
}

template<typename DefaultStringMatcher = void, typename... Matchers>
constexpr auto alt(Matchers&&... matchers)
{
    constexpr auto convert = []<typename T>(T&& t) {
        if constexpr (std::is_convertible_v<T&&, std::string_view>)
            return DefaultStringMatcher(std::forward<T>(t));
        else
            return std::forward<T>(t);
    };
    return [... matchers = convert(std::forward<Matchers>(matchers))]<typename It>(
               It& b, const It& e) noexcept { return ((matchers(b, e) || ...)); };
}

template<typename Matcher>
constexpr auto opt(Matcher&& matcher)
{
    return [matcher = std::forward<Matcher>(matcher)]<typename It>(It& b, const It& e) noexcept {
        matcher(b, e);
        return true;
    };
}

class start_of_next_line
{
public:
    template<typename It>
    constexpr bool operator()(It& b, const It&) const noexcept
    {
        return !same_line(std::prev(b), b);
    }
};

template<typename It>
class start_of_line
{
    It start;

public:
    start_of_line(It start)
        : start(std::move(start))
    {}
    constexpr bool operator()(It& b, const It& e) const noexcept { return b == start || start_of_next_line()(b, e); }
};

class end
{
public:
    template<typename It>
    constexpr bool operator()(It& b, const It& e) const noexcept
    {
        return b == e;
    }
};

template<typename It, typename Matcher>
constexpr auto capture(std::pair<It, It>& capture, Matcher&& matcher)
{
    return [&capture, matcher = std::forward<Matcher>(matcher)]<typename It>(It& b, const It& e) noexcept {
        auto work = b;
        if (matcher(b, e))
        {
            capture = std::make_pair(work, b);
            return true;
        }
        else
            return false;
    };
}

template<typename It, typename Matcher>
constexpr auto capture(std::optional<std::pair<It, It>>& capture, Matcher&& matcher)
{
    return [&capture, matcher = std::forward<Matcher>(matcher)]<typename It>(It& b, const It& e) noexcept {
        auto work = b;
        if (matcher(b, e))
        {
            capture.emplace(std::make_pair(work, b));
            return true;
        }
        else
        {
            capture.reset();
            return false;
        }
    };
}

} // namespace hlasm_plugin::utils::text_matchers

#endif
