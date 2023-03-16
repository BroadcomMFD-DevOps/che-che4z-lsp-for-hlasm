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

#include <map>
#include <memory>
#include <span>

#include "file_impl.h"
#include "utils/content_loader.h"
#include "utils/path.h"
#include "utils/path_conversions.h"
#include "utils/platform.h"

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


struct file_manager_impl::mapped_file : std::enable_shared_from_this<mapped_file>
{
    file_impl file;
    file_manager_impl& fm;
    decltype(file_manager_impl::m_files)::const_iterator it;
    std::shared_ptr<void> editing_self_reference;

    mapped_file(const file_location& file_name, file_manager_impl& fm)
        : file(file_name)
        , fm(fm)
    {}

    mapped_file(const file_impl& file, file_manager_impl& fm)
        : file(file)
        , fm(fm)
    {}

    ~mapped_file()
    {
        if (it != fm.m_files.end())
        {
            std::lock_guard guard(fm.files_mutex);
            fm.m_files.erase(it);
        }
    }
};

std::shared_ptr<file> file_manager_impl::add_file(const file_location& file_name)
{
    std::lock_guard guard(files_mutex);
    return add_file_unsafe(file_name).first;
}

std::pair<std::shared_ptr<file_impl>, decltype(file_manager_impl::m_files)::iterator>
file_manager_impl::add_file_unsafe(const file_location& file_name)
{
    std::shared_ptr<mapped_file> mf;
    auto it = m_files.find(file_name);
    if (it == m_files.end())
    {
        mf = std::make_shared<mapped_file>(file_name, *this);
        mf->it = it = m_files.emplace(file_name, mf.get()).first;
    }
    else
        mf = it->second->shared_from_this();

    return { std::shared_ptr<file_impl>(std::move(mf), &mf->file), it };
}

std::optional<std::string> file_manager_impl::get_file_content(const utils::resource::resource_location& file_name)
{
    std::shared_ptr<file_impl> file;

    std::lock_guard guard(files_mutex);

    file = add_file_unsafe(file_name).first;

    std::optional<std::string> result(file->get_text());
    if (file->is_bad())
        result.reset();

    return result;
}

std::shared_ptr<file> file_manager_impl::find(const utils::resource::resource_location& key) const
{
    std::lock_guard guard(files_mutex);
    auto ret = m_files.find(key);
    if (ret == m_files.end())
        return nullptr;

    auto mf = ret->second->shared_from_this();
    return std::shared_ptr<file>(std::move(mf), &mf->file);
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

std::shared_ptr<file_impl> file_manager_impl::prepare_edited_file_for_change(mapped_file*& file)
{
    auto old_file = file->shared_from_this();
    auto new_file = std::make_shared<mapped_file>(old_file->file, *this);

    old_file->it = m_files.end();
    old_file->editing_self_reference.reset();

    file = new_file.get();

    new_file->editing_self_reference = new_file;

    return std::shared_ptr<file_impl>(std::move(new_file), &new_file->file);
}

open_file_result file_manager_impl::did_open_file(
    const file_location& document_loc, version_t version, std::string text)
{
    std::lock_guard guard(files_mutex);

    auto [file, it] = add_file_unsafe(document_loc);

    assert(!file->get_lsp_editing());
    if (file.use_count() == 1)
    {
        it->second->editing_self_reference = file;
        file->did_open(std::move(text), version);
        return open_file_result::changed_content;
    }

    if (file->is_text_loaded() && file->get_text() != text)
        file = prepare_edited_file_for_change(it->second);
    return file->did_open(std::move(text), version);
}

void file_manager_impl::did_change_file(
    const file_location& document_loc, version_t, const document_change* changes_start, size_t ch_size)
{
    if (!ch_size)
        return;

    std::lock_guard guard(files_mutex);

    auto it = m_files.find(document_loc);
    if (it == m_files.end())
        return; // if the file does not exist, no action is taken

    auto file = std::shared_ptr<file_impl>(it->second->shared_from_this(), &it->second->file);
    if (file.use_count() > 1 + (it->second->editing_self_reference != nullptr))
        file = prepare_edited_file_for_change(it->second);

    auto last_whole = changes_start + ch_size;
    while (last_whole != changes_start)
    {
        --last_whole;
        if (last_whole->whole)
            break;
    }

    for (const auto& change : std::span(last_whole, changes_start + ch_size))
    {
        std::string text_s(change.text, change.text_length);
        if (change.whole)
            file->did_change(std::move(text_s));
        else
            file->did_change(change.change_range, std::move(text_s));
    }
}

void file_manager_impl::did_close_file(const file_location& document_loc)
{
    std::shared_ptr<void> to_release;

    std::lock_guard guard(files_mutex);
    auto file = m_files.find(document_loc);
    if (file == m_files.end())
        return;

    std::swap(to_release, file->second->editing_self_reference);
    if (!file->second->file.is_text_loaded()
        || file->second->file.get_text() == utils::resource::load_text(file->second->file.get_location()))
        file->second->file.did_close(); // close the file externally, content is accurate
    else
    {
        file->second->it = m_files.end();
        m_files.erase(file);
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
    std::lock_guard guard(files_mutex);
    auto f = m_files.find(document_loc);
    if (f == m_files.end())
        return open_file_result::identical;

    if (f->second->file.get_lsp_editing())
        return open_file_result::identical;

    // the mere existence of m_files entry indicates live handle

    auto result = open_file_result::identical;

    auto current_text = utils::resource::load_text(document_loc);

    if (f->second->file.is_bad())
    {
        if (current_text.has_value())
            result = open_file_result::changed_content;
    }
    else if (!f->second->file.is_text_loaded())
        result = open_file_result::changed_content;
    else if (f->second->file.get_text() != current_text.value())
        result = open_file_result::changed_content;

    if (result == open_file_result::changed_content)
    {
        f->second->it = m_files.end();
        m_files.erase(f);
    }

    return result;
}

} // namespace hlasm_plugin::parser_library::workspaces
