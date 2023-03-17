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

#include "file_manager_impl.h"

#include <atomic>
#include <map>
#include <memory>
#include <span>
#include <variant>

#include "utils/content_loader.h"
#include "utils/path.h"
#include "utils/path_conversions.h"
#include "utils/platform.h"

namespace {
std::atomic<hlasm_plugin::parser_library::version_t> global_version = 0;
auto next_global_version() { return global_version.fetch_add(1, std::memory_order_relaxed) + 1; }
} // namespace

namespace hlasm_plugin::parser_library::workspaces {

size_t index_from_position(std::string_view text, const std::vector<size_t>& line_indices, position loc)
{
    size_t end = (size_t)loc.column;
    if (loc.line >= line_indices.size())
        return text.size();
    size_t i = line_indices[loc.line];
    size_t utf16_counter = 0;

    while (utf16_counter < end && i < text.size())
    {
        unsigned char c = text[i];
        if (!utils::utf8_one_byte_begin(c))
        {
            const auto cs = utils::utf8_prefix_sizes[c];

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

void find_newlines(std::vector<size_t>& output, std::string_view text)
{
    static constexpr std::string_view nl("\r\n");
    for (auto i = text.find_first_of(nl); i != std::string_view::npos; i = text.find_first_of(nl, i))
    {
        i += 1 + text.substr(i).starts_with(nl);
        output.push_back(i);
    }
}

std::vector<size_t> create_line_indices(std::string_view text)
{
    std::vector<size_t> ret;
    create_line_indices(ret, text);
    return ret;
}

void create_line_indices(std::vector<size_t>& output, std::string_view text)
{
    output.clear();
    output.push_back(0);
    find_newlines(output, text);
}

void apply_text_diff(std::string& text, std::vector<size_t>& lines, range r, std::string_view replacement)
{
    size_t range_end_line = (size_t)r.end.line;
    size_t range_start_line = (size_t)r.start.line;

    size_t begin = index_from_position(text, lines, r.start);
    size_t end = index_from_position(text, lines, r.end);

    text.replace(begin, end - begin, replacement);

    std::vector<size_t> new_lines;
    find_newlines(new_lines, replacement);

    size_t old_lines_count = range_end_line - range_start_line;
    size_t new_lines_count = new_lines.size();

    size_t char_diff = replacement.size() - (end - begin);

    // add or remove lines depending on the difference
    if (new_lines_count > old_lines_count)
    {
        size_t diff = new_lines_count - old_lines_count;
        lines.insert(lines.end(), diff, 0);

        for (size_t i = lines.size() - 1; i > range_end_line + diff; --i)
        {
            lines[i] = lines[i - diff] + char_diff;
        }
    }
    else
    {
        size_t diff = old_lines_count - new_lines_count;

        for (size_t i = range_start_line + 1 + new_lines_count; i < lines.size() - diff; ++i)
        {
            lines[i] = lines[i + diff] + char_diff;
        }

        for (size_t i = 0; i < diff; ++i)
            lines.pop_back();
    }


    for (size_t i = range_start_line + 1; i <= range_start_line + new_lines_count; ++i)
    {
        lines[i] = new_lines[i - (size_t)range_start_line - 1] + begin;
    }
}


struct file_manager_impl::mapped_file final : std::enable_shared_from_this<mapped_file>, file
{
    file_location m_location;
    std::string m_text;
    struct file_error
    {};
    std::optional<file_error> m_error;
    // Array of "pointers" to m_text where lines start.
    std::vector<size_t> m_lines;

    file_manager_impl& m_fm;

    version_t m_lsp_version = 0;

    version_t m_version = next_global_version();

    std::shared_ptr<void> m_editing_self_reference;
    decltype(file_manager_impl::m_files)::const_iterator m_it = m_fm.m_files.end();
    decltype(file_manager_impl::m_files_closed)::const_iterator m_closed_it = m_fm.m_files_closed.end();

    mapped_file(const file_location& file_name, file_manager_impl& fm, std::string text)
        : m_location(file_name)
        , m_text(std::move(text))
        , m_lines(create_line_indices(m_text))
        , m_fm(fm)
    {}

    mapped_file(const file_location& file_name, file_manager_impl& fm, file_error error)
        : m_location(file_name)
        , m_error(std::move(error))
        , m_fm(fm)
    {}

    mapped_file(const mapped_file& that)
        : m_location(that.m_location)
        , m_text(that.m_text)
        , m_error(that.m_error)
        , m_lines(that.m_lines)
        , m_fm(that.m_fm)
        , m_lsp_version(that.m_lsp_version)
    {}

    ~mapped_file()
    {
        if (m_it == m_fm.m_files.end() && m_closed_it == m_fm.m_files_closed.end())
            return;

        std::lock_guard guard(m_fm.files_mutex);
        if (m_it != m_fm.m_files.end())
            m_fm.m_files.erase(m_it);
        if (m_closed_it != m_fm.m_files_closed.end())
            m_fm.m_files_closed.erase(m_closed_it);
    }

    // Inherited via file
    const file_location& get_location() override { return m_location; }
    const std::string& get_text() override { return m_text; }
    bool get_lsp_editing() const override { return m_editing_self_reference != nullptr; }
    version_t get_version() const override { return m_version; }
    bool error() const override { return m_error.has_value(); }
};

class : public external_file_reader
{
    std::optional<std::string> load_text(const utils::resource::resource_location& document_loc) final
    {
        return utils::resource::load_text(document_loc);
    }
} default_reader;

file_manager_impl::file_manager_impl()
    : file_manager_impl(default_reader)
{}

file_manager_impl::file_manager_impl(external_file_reader& file_reader)
    : m_file_reader(&file_reader)
{}

std::shared_ptr<file> file_manager_impl::add_file(const file_location& file_name)
{
    std::unique_lock lock(files_mutex);
    auto it = m_files.find(file_name);
    if (it != m_files.end())
        return it->second->shared_from_this();

    lock.unlock();

    auto loaded_text = m_file_reader->load_text(file_name);

    lock.lock();

    it = m_files.find(file_name);
    if (it != m_files.end())
        return it->second->shared_from_this();

    auto result = revive_file(file_name, loaded_text);

    if (!result)
        result = loaded_text.has_value() ? std::make_shared<mapped_file>(file_name, *this, loaded_text.value())
                                         : std::make_shared<mapped_file>(file_name, *this, mapped_file::file_error());

    result->m_it = m_files.try_emplace(file_name, result.get()).first;

    return result;
}


std::shared_ptr<file_manager_impl::mapped_file> file_manager_impl::revive_file(
    const utils::resource::resource_location& file_name, std::optional<std::string_view> expected_text)
{
    auto closed = m_files_closed.find(file_name);
    if (closed == m_files_closed.end())
        return {};

    auto result = closed->second.lock();
    assert(result); // it should have been removed from m_files_closed on end of life

    result->m_closed_it = m_files_closed.end();
    m_files_closed.erase(closed);

    if (expected_text.has_value() && result->m_text == expected_text.value())
        return result;
    else
        return {};
}

std::optional<std::string> file_manager_impl::get_file_content(const utils::resource::resource_location& file_name)
{
    auto f = add_file(file_name);
    if (f->error())
        return std::nullopt;
    else
        return f->get_text();
}

std::shared_ptr<file> file_manager_impl::find(const utils::resource::resource_location& key) const
{
    std::lock_guard guard(files_mutex);
    auto ret = m_files.find(key);
    if (ret == m_files.end())
        return nullptr;

    return ret->second->shared_from_this();
}

list_directory_result file_manager_impl::list_directory_files(const utils::resource::resource_location& directory) const
{
    return utils::resource::list_directory_files(directory);
}

list_directory_result file_manager_impl::list_directory_subdirs_and_symlinks(
    const utils::resource::resource_location& directory) const
{
    return utils::resource::list_directory_subdirs_and_symlinks(directory);
}

std::string file_manager_impl::canonical(const utils::resource::resource_location& res_loc, std::error_code& ec) const
{
    return utils::resource::canonical(res_loc, ec);
}

open_file_result file_manager_impl::did_open_file(
    const file_location& document_loc, version_t version, std::string new_text)
{
    std::shared_ptr<void> to_release;

    std::lock_guard guard(files_mutex);

    auto it = m_files.find(document_loc);
    if (it == m_files.end() || it->second->m_error || it->second->m_text != new_text)
    {
        std::shared_ptr<mapped_file> result;

        if (it != m_files.end())
        {
            std::swap(to_release, it->second->m_editing_self_reference);
            it->second->m_it = m_files.end();
        }
        else
            result = revive_file(document_loc, new_text);

        if (!result)
            result = std::make_shared<mapped_file>(document_loc, *this, new_text);

        result->m_lsp_version = version;

        result->m_it = m_files.insert_or_assign(document_loc, result.get()).first;
        result->m_editing_self_reference = result;

        return open_file_result::changed_content;
    }
    else
    {
        it->second->m_lsp_version = version;
        it->second->m_editing_self_reference = it->second->shared_from_this();

        return open_file_result::changed_lsp;
    }
}

void file_manager_impl::did_change_file(
    const file_location& document_loc, version_t, const document_change* changes_start, size_t ch_size)
{
    if (!ch_size)
        return;

    std::shared_ptr<void> to_release;

    std::lock_guard guard(files_mutex);

    auto it = m_files.find(document_loc);
    if (it == m_files.end())
        return; // if the file does not exist, no action is taken

    auto file = it->second->shared_from_this();
    if (file.use_count() > 1 + (it->second->m_editing_self_reference != nullptr))
    {
        // this file is already being used by others
        auto new_file = std::make_shared<mapped_file>(*file);

        new_file->m_editing_self_reference = new_file;
        new_file->m_it = it;

        it->second->m_it = m_files.end();
        std::swap(to_release, it->second->m_editing_self_reference);

        it->second = new_file.get();

        std::swap(file, new_file);
    }

    // now we can be sure that we are the only ones who have access to file

    auto last_whole = changes_start + ch_size;
    while (last_whole != changes_start)
    {
        --last_whole;
        if (last_whole->whole)
            break;
    }

    if (last_whole->whole)
    {
        file->m_text = std::string_view(last_whole->text, last_whole->text_length);
        ++last_whole;
    }

    for (const auto& change : std::span(last_whole, changes_start + ch_size))
    {
        std::string_view text_s(change.text, change.text_length);
        apply_text_diff(file->m_text, file->m_lines, change.change_range, text_s);
    }

    file->m_lsp_version += ch_size;
    file->m_version = next_global_version();
}

void file_manager_impl::did_close_file(const file_location& document_loc)
{
    std::shared_ptr<void> to_release;

    std::lock_guard guard(files_mutex);

    auto it = m_files.find(document_loc);
    if (it == m_files.end())
        return;

    auto file = it->second->shared_from_this();
    std::swap(to_release, file->m_editing_self_reference);
    file->m_it = m_files.end();
    m_files.erase(it);

    if (file.use_count() > 1 + (to_release != nullptr) && !file->m_error)
    {
        // somebody still uses the file, save it for possible re-open
        file->m_closed_it = m_files_closed.insert_or_assign(document_loc, file).first;
    }
}

bool file_manager_impl::dir_exists(const utils::resource::resource_location& dir_loc) const
{
    return utils::resource::dir_exists(dir_loc);
}

void file_manager_impl::put_virtual_file(
    unsigned long long id, std::string_view text, utils::resource::resource_location related_workspace)
{
    std::lock_guard guard(virtual_files_mutex);
    m_virtual_files.try_emplace(id, text, std::move(related_workspace));
}

void file_manager_impl::remove_virtual_file(unsigned long long id)
{
    std::lock_guard guard(virtual_files_mutex);
    m_virtual_files.erase(id);
}

std::string file_manager_impl::get_virtual_file(unsigned long long id) const
{
    std::lock_guard guard(virtual_files_mutex);
    if (auto it = m_virtual_files.find(id); it != m_virtual_files.end())
        return it->second.text;
    return {};
}

utils::resource::resource_location file_manager_impl::get_virtual_file_workspace(unsigned long long id) const
{
    std::lock_guard guard(virtual_files_mutex);
    if (auto it = m_virtual_files.find(id); it != m_virtual_files.end())
        return it->second.related_workspace;
    return utils::resource::resource_location();
}

open_file_result file_manager_impl::update_file(const file_location& document_loc)
{
    std::unique_lock lock(files_mutex);
    auto f = m_files.find(document_loc);
    if (f == m_files.end())
        return open_file_result::identical;

    if (f->second->get_lsp_editing())
        return open_file_result::identical;

    lock.unlock();

    auto current_text = m_file_reader->load_text(document_loc);

    lock.lock();

    f = m_files.find(document_loc);
    if (f == m_files.end())
        return open_file_result::identical;

    if (f->second->get_lsp_editing())
        return open_file_result::identical;

    if (f->second->m_error.has_value() && !current_text.has_value() || f->second->m_text == current_text)
        return open_file_result::identical;

    f->second->m_it = m_files.end();
    m_files.erase(f);
    return open_file_result::changed_content;
}

} // namespace hlasm_plugin::parser_library::workspaces
