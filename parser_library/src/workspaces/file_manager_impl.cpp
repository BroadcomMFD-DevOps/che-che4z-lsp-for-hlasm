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
    struct file_error
    {};
    file_location m_location;
    std::variant<std::monostate, file_error, std::string> m_text;
    // Array of "pointers" to text_ where lines start.
    std::vector<size_t> m_lines;

    version_t m_lsp_version = 0;
    version_t m_version = next_global_version();

    file_manager_impl& m_fm;
    decltype(file_manager_impl::m_files)::const_iterator m_it;
    std::shared_ptr<void> m_editing_self_reference;

    mapped_file(const file_location& file_name, file_manager_impl& fm)
        : m_location(file_name)
        , m_fm(fm)
    {}

    mapped_file(const mapped_file& that)
        : m_location(that.m_location)
        , m_text(that.m_text)
        , m_lines(that.m_lines)
        , m_lsp_version(that.m_lsp_version)
        , m_fm(that.m_fm)
        , m_it(that.m_it)
    {}

    ~mapped_file()
    {
        if (m_it != m_fm.m_files.end())
        {
            std::lock_guard guard(m_fm.files_mutex);
            m_fm.m_files.erase(m_it);
        }
    }

    // Inherited via file
    const file_location& get_location() override { return m_location; }
    const std::string& get_text() override
    {
        static std::string empty_string;
        if (std::holds_alternative<std::monostate>(m_text))
            load_text();

        if (std::holds_alternative<std::string>(m_text))
            return std::get<std::string>(m_text);
        else
            return empty_string;
    }
    bool get_lsp_editing() const override { return m_editing_self_reference != nullptr; }
    version_t get_version() const override { return m_version; }

    update_file_result load_text()
    {
        if (auto loaded_text = utils::resource::load_text(m_location); loaded_text.has_value())
        {
            bool was_up_to_date = !std::holds_alternative<std::monostate>(m_text);
            bool identical =
                std::holds_alternative<std::string>(m_text) && std::get<std::string>(m_text) == loaded_text.value();
            if (!identical)
            {
                replace_text(utils::replace_non_utf8_chars(loaded_text.value()));
            }
            return identical && was_up_to_date ? update_file_result::identical : update_file_result::changed;
        }

        m_text.emplace<file_error>();
        m_version = next_global_version();

        // TODO Figure out how to present this error in VSCode.
        // Also think about the lifetime of the error as it seems that it will stay in Problem panel forever
        // add_diagnostic(diagnostic_s::error_W0001(file_location_.to_presentable()));

        return update_file_result::bad;
    }

    void replace_text(std::string&& s)
    {
        m_text = std::move(s);
        create_line_indices(m_lines, std::get<std::string>(m_text));
        m_version = next_global_version();
    }
    void replace_text(std::string_view s)
    {
        m_text.emplace<std::string>(std::move(s));
        create_line_indices(m_lines, std::get<std::string>(m_text));
        m_version = next_global_version();
    }
};

std::shared_ptr<file> file_manager_impl::add_file(const file_location& file_name)
{
    std::lock_guard guard(files_mutex);
    return add_file_unsafe(file_name).first;
}

std::pair<std::shared_ptr<file_manager_impl::mapped_file>, decltype(file_manager_impl::m_files)::iterator>
file_manager_impl::add_file_unsafe(const file_location& file_name)
{
    std::shared_ptr<mapped_file> mf;
    auto it = m_files.find(file_name);
    if (it == m_files.end())
    {
        mf = std::make_shared<mapped_file>(file_name, *this);
        mf->m_it = it = m_files.emplace(file_name, mf.get()).first;
    }
    else
        mf = it->second->shared_from_this();

    return { std::move(mf), it };
}

std::optional<std::string> file_manager_impl::get_file_content(const utils::resource::resource_location& file_name)
{
    std::shared_ptr<mapped_file> file;

    std::lock_guard guard(files_mutex);

    file = add_file_unsafe(file_name).first;

    std::optional<std::string> result(file->get_text());
    if (std::holds_alternative<mapped_file::file_error>(file->m_text))
        result.reset();

    return result;
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

std::shared_ptr<file_manager_impl::mapped_file> file_manager_impl::prepare_edited_file_for_change(mapped_file*& file)
{
    auto old_file = file->shared_from_this();
    auto new_file = std::make_shared<mapped_file>(*old_file);

    old_file->m_it = m_files.end();
    old_file->m_editing_self_reference.reset();

    file = new_file.get();

    new_file->m_editing_self_reference = new_file;

    return new_file;
}

open_file_result file_manager_impl::did_open_file(
    const file_location& document_loc, version_t version, std::string new_text)
{
    std::lock_guard guard(files_mutex);

    auto [file, it] = add_file_unsafe(document_loc);

    auto result = open_file_result::changed_lsp;
    if (file.use_count() == 1 + file->get_lsp_editing())
    {
        it->second->m_editing_self_reference = file;
        file->replace_text(std::move(new_text));
        result = open_file_result::changed_content;
    }
    else if (!std::holds_alternative<std::string>(file->m_text) || std::get<std::string>(file->m_text) != new_text)
    {
        file = prepare_edited_file_for_change(it->second);
        file->replace_text(std::move(new_text));
        result = open_file_result::changed_content;
    }

    file->m_lsp_version = version;
    file->m_editing_self_reference = file;

    return result;
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

    auto file = it->second->shared_from_this();
    if (file.use_count() > 1 + (it->second->m_editing_self_reference != nullptr))
        file = prepare_edited_file_for_change(it->second);

    auto last_whole = changes_start + ch_size;
    while (last_whole != changes_start)
    {
        --last_whole;
        if (last_whole->whole)
            break;
    }

    if (!std::holds_alternative<std::string>(file->m_text)) // this should never really happen
        file->m_text.emplace<std::string>();

    for (const auto& change : std::span(last_whole, changes_start + ch_size))
    {
        std::string_view text_s(change.text, change.text_length);
        if (change.whole)
        {
            file->replace_text(text_s);
            ++file->m_lsp_version;
        }
        else
        {
            apply_text_diff(std::get<std::string>(file->m_text), file->m_lines, change.change_range, text_s);
        }
    }
    file->m_lsp_version += ch_size;
    file->m_version = next_global_version();
}

void file_manager_impl::did_close_file(const file_location& document_loc)
{
    std::shared_ptr<void> to_release;

    std::lock_guard guard(files_mutex);
    auto file = m_files.find(document_loc);
    if (file == m_files.end())
        return;

    std::swap(to_release, file->second->m_editing_self_reference);
    if (std::holds_alternative<std::string>(file->second->m_text)
        && std::get<std::string>(file->second->m_text) != utils::resource::load_text(file->second->m_location))
    {
        file->second->m_it = m_files.end();
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

    if (f->second->get_lsp_editing())
        return open_file_result::identical;

    // the mere existence of m_files entry indicates live handle

    auto result = open_file_result::identical;

    auto current_text = utils::resource::load_text(document_loc);

    if (std::holds_alternative<mapped_file::file_error>(f->second->m_text))
    {
        if (current_text.has_value())
            result = open_file_result::changed_content;
    }
    else if (std::holds_alternative<std::monostate>(f->second->m_text))
        result = open_file_result::changed_content;
    else if (std::get<std::string>(f->second->m_text) != current_text.value())
        result = open_file_result::changed_content;

    if (result == open_file_result::changed_content)
    {
        f->second->m_it = m_files.end();
        m_files.erase(f);
    }

    return result;
}

} // namespace hlasm_plugin::parser_library::workspaces
