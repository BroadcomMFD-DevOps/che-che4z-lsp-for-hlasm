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


#include "file_impl.h"

#include <atomic>
#include <stdexcept>

#include "file_manager.h"
#include "utils/content_loader.h"
#include "utils/unicode_text.h"

std::atomic<hlasm_plugin::parser_library::version_t> global_version = 0;

namespace hlasm_plugin::parser_library::workspaces {

file_impl::file_impl(file_location location)
    : file_location_(std::move(location))
    , text_()
    , version_(++global_version)
{}

const file_location& file_impl::get_location() { return file_location_; }

const std::string& file_impl::get_text()
{
    if (!up_to_date_)
        load_text();
    return get_text_ref();
}

update_file_result file_impl::load_text()
{
    if (auto loaded_text = utils::resource::load_text(file_location_); loaded_text.has_value())
    {
        bool was_up_to_date = up_to_date_;
        bool identical = get_text_ref() == loaded_text.value();
        if (!identical)
        {
            replace_text(utils::replace_non_utf8_chars(loaded_text.value()));
        }
        up_to_date_ = true;
        bad_ = false;
        return identical && was_up_to_date ? update_file_result::identical : update_file_result::changed;
    }

    replace_text("");
    up_to_date_ = false;
    bad_ = true;

    // TODO Figure out how to present this error in VSCode.
    // Also think about the lifetime of the error as it seems that it will stay in Problem panel forever
    // add_diagnostic(diagnostic_s::error_W0001(file_location_.to_presentable()));

    return update_file_result::bad;
}

// adds positions of newlines into vector 'lines'
size_t find_newlines(std::string_view text, std::vector<size_t>& lines)
{
    size_t before = lines.size();
    bool was_r = false;
    for (size_t i = 0; i < text.size(); ++i)
    {
        char ch = text[i];
        if (was_r)
        {
            if (ch == '\n')
            {
                lines.push_back(i + 1);
                was_r = false;
            }
            else if (ch == '\r')
                lines.push_back(i);
            else
            {
                lines.push_back(i);
                was_r = false;
            }
        }
        else
        {
            if (ch == '\n')
                lines.push_back(i + 1);
            else if (ch == '\r')
                was_r = true;
        }
    }

    if (was_r)
        lines.push_back(text.size());

    return lines.size() - before;
}

open_file_result file_impl::did_open(std::string new_text, version_t version)
{
    bool identical = get_text_ref() == new_text;
    if (!identical || bad_)
    {
        replace_text(std::move(new_text));
    }

    lsp_version_ = version;

    lines_ind_.clear();
    lines_ind_.push_back(0);

    find_newlines(get_text_ref(), lines_ind_);
    up_to_date_ = true;
    bad_ = false;
    editing_ = true;

    return identical ? open_file_result::changed_lsp : open_file_result::changed_content;
}

bool file_impl::get_lsp_editing() const { return editing_; }


// applies a change to the text and updates line beginnings
void file_impl::did_change(range range, std::string new_text)
{
    size_t range_end_line = (size_t)range.end.line;
    size_t range_start_line = (size_t)range.start.line;

    size_t begin = index_from_position(get_text_ref(), lines_ind_, range.start);
    size_t end = index_from_position(get_text_ref(), lines_ind_, range.end);

    text_.replace(begin, end - begin, new_text);
    version_ = ++global_version;
    ++lsp_version_;

    std::vector<size_t> new_lines;

    find_newlines(new_text, new_lines);

    size_t old_lines_count = range_end_line - range_start_line;
    size_t new_lines_count = new_lines.size();

    size_t char_diff = new_text.size() - (end - begin);

    // add or remove lines depending on the difference
    if (new_lines_count > old_lines_count)
    {
        size_t diff = new_lines_count - old_lines_count;
        lines_ind_.insert(lines_ind_.end(), diff, 0);

        for (size_t i = lines_ind_.size() - 1; i > range_end_line + diff; --i)
        {
            lines_ind_[i] = lines_ind_[i - diff] + char_diff;
        }
    }
    else
    {
        size_t diff = old_lines_count - new_lines_count;

        for (size_t i = range_start_line + 1 + new_lines_count; i < lines_ind_.size() - diff; ++i)
        {
            lines_ind_[i] = lines_ind_[i + diff] + char_diff;
        }

        for (size_t i = 0; i < diff; ++i)
            lines_ind_.pop_back();
    }


    for (size_t i = range_start_line + 1; i <= range_start_line + new_lines_count; ++i)
    {
        lines_ind_[i] = new_lines[i - (size_t)range_start_line - 1] + begin;
    }
}

void file_impl::did_change(std::string new_text)
{
    replace_text(std::move(new_text));
    lines_ind_.clear();
    lines_ind_.push_back(0);
    find_newlines(get_text_ref(), lines_ind_);
    ++lsp_version_;
}

void file_impl::did_close() { editing_ = false; }

const std::string& file_impl::get_text_ref() { return text_; }

version_t file_impl::get_lsp_version() { return lsp_version_; }
version_t file_impl::get_version() { return version_; }

void file_impl::replace_text(std::string s)
{
    text_ = std::move(s);
    version_ = ++global_version;
}

update_file_result file_impl::update_and_get_bad()
{
    // If user is editing file through LSP, do not load from disk.
    if (editing_)
        return update_file_result::identical;

    return load_text();
}

size_t file_impl::index_from_position(std::string_view text, const std::vector<size_t>& line_indices, position loc)
{
    size_t end = (size_t)loc.column;
    if (loc.line >= line_indices.size())
        return text.size();
    size_t i = line_indices[loc.line];
    size_t utf16_counter = 0;

    while (utf16_counter < end && i < text.size())
    {
        if (!utils::utf8_one_byte_begin(text[i]))
        {
            const auto cs = utils::utf8_prefix_sizes[(unsigned char)text[i]];

            if (!cs.utf8)
                throw std::runtime_error("The text of the file is not in utf-8."); // WRONG UTF-8 input

            i += cs.utf8;
            utf16_counter += cs.utf16;
        }
        else
        {
            ++i;
            ++utf16_counter;
        }
    }
    return i;
}

std::vector<size_t> file_impl::create_line_indices(std::string_view text)
{
    std::vector<size_t> ret;
    ret.push_back(0);
    find_newlines(text, ret);
    return ret;
}


} // namespace hlasm_plugin::parser_library::workspaces
