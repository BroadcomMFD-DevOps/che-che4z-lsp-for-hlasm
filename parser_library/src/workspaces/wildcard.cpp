/*
 * Copyright (c) 2019 Broadcom.
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

#include "wildcard.h"

#include "utils/platform.h"

namespace hlasm_plugin::parser_library::workspaces {
namespace {
// used for wildcard to regex conversions
const std::regex escape("(\\(|\\[|\\{|\\\\|\\^|\\-|\\=|\\$|\\!|\\||\\]|\\}|\\)|\\.)");
const std::regex question("\\?");
const std::regex nongreedy("(\\*|\\+)");
const std::regex slash("\\\\");
const std::regex file_scheme_windows("^file:///([A-Za-z])(?::|%3[aA])");

const std::string single_url_char_matcher = []() {
    const std::string utf_8_continuation_matcher = "(?:%[89abAB][0-9a-fA-F])";
    const std::string utf_8_1_byte_matcher = "%[0-7][0-9a-fA-F]";
    const std::string utf_8_2_byte_matcher = "%[cCdD][0-9a-fA-F]" + utf_8_continuation_matcher;
    const std::string utf_8_3_byte_matcher = "%[eE][0-9a-fA-F]" + utf_8_continuation_matcher + "{2}";
    const std::string utf_8_4_byte_matcher = "%[fF][0-7]" + utf_8_continuation_matcher + "{3}";

    const std::string utf_8_char_matcher =
        utf_8_4_byte_matcher + "|" + utf_8_3_byte_matcher + "|" + utf_8_2_byte_matcher + "|" + utf_8_1_byte_matcher;

    return "(?:[^%//]|" + utf_8_char_matcher + ")";
}();

std::pair<size_t, char> match_windows_uri_with_drive(const std::string& input)
{
    if (std::smatch matches; std::regex_search(input, matches, file_scheme_windows))
        return { matches[0].length(), matches[1].str()[0] };
    else
        return { 0, '\0' };
}

std::pair<size_t, std::string> preprocess_uri_with_windows_drive_letter_regex_string(const std::string& input)
{
    auto [match_length, drive_letter] = match_windows_uri_with_drive(input);

    if (match_length == 0)
        return { 0, "" };

    std::string r;

    // Append windows file path (e.g. ^file:///[cC](?::|%3[aA]))
    r.append("^file:///[");
    r.push_back(static_cast<char>(tolower(drive_letter)));
    r.push_back(static_cast<char>(toupper(drive_letter)));
    r.append("](?::|%3[aA])");

    return { match_length, r };
}
} // namespace

std::regex wildcard2regex(std::string wildcard)
{
    // change of double backslash to forward slash
    wildcard = std::regex_replace(wildcard, slash, "/");
    wildcard = std::regex_replace(wildcard, escape, "\\$1");
    wildcard = std::regex_replace(wildcard, question, single_url_char_matcher);
    wildcard = std::regex_replace(wildcard, nongreedy, ".$1?");

    if (auto [match_length, regex] = preprocess_uri_with_windows_drive_letter_regex_string(wildcard); match_length != 0)
    {
        regex.append(wildcard.substr(match_length));

        wildcard = std::move(regex);
    }

    return std::regex(wildcard);
}

std::regex pathmask_to_regex(const std::string& input)
{
    // URI mask shouldn't care about Windows Drive letter case
    auto [match_length, r] = preprocess_uri_with_windows_drive_letter_regex_string(input);
    r.reserve(input.size());

    // Remove prefix of the preprocessed windows drive letter
    std::string_view s = input;
    s.remove_prefix(match_length);

    bool path_started = false;
    while (!s.empty())
    {
        switch (auto c = s.front())
        {
            case '*':
                if (s.starts_with("**/"))
                {
                    if (path_started)
                    {
                        path_started = false;
                        r.append("[^/]*[/]");
                    }
                    r.append("(?:.*/)?");
                    s.remove_prefix(3);
                }
                else if (s.starts_with("**"))
                {
                    r.append(".*");
                    s.remove_prefix(2);
                }
                else if (s.starts_with("*/"))
                {
                    path_started = false;
                    r.append("[^/]*[/]");
                    s.remove_prefix(2);
                }
                else
                {
                    r.append("[^/]*");
                    s.remove_prefix(1);
                }
                break;

            case '/':
                path_started = false;
                r.append("[/]");
                s.remove_prefix(1);
                break;

            case '?':
                path_started = true;
                r.append(single_url_char_matcher);
                s.remove_prefix(1);
                break;
            case '^':
            case '$':
            case '+':
            case '.':
            case '(':
            case ')':
            case '|':
            case '{':
            case '}':
            case '[':
            case ']':
                r.push_back('\\');
                [[fallthrough]];
            default:
                path_started = true;
                r.push_back(c);
                s.remove_prefix(1);
                break;
        }
    }

    return std::regex(r);
}

} // namespace hlasm_plugin::parser_library::workspaces