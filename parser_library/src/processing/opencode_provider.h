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

#ifndef PROCESSING_OPENCODE_PROVIDER_H
#define PROCESSING_OPENCODE_PROVIDER_H

#include <concepts>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "context/source_snapshot.h"
#include "lexing/logical_line.h"
#include "lexing/string_with_newlines.h"
#include "preprocessor.h"
#include "range.h"
#include "statement_providers/statement_provider.h"
#include "utils/unicode_text.h"
#include "virtual_file_monitor.h"


namespace hlasm_plugin::utils {
class task;
template<std::move_constructible T>
class value_task;
} // namespace hlasm_plugin::utils

namespace hlasm_plugin::parser_library {
class analyzer_options;
class diagnosable_ctx;
class parse_lib_provider;
} // namespace hlasm_plugin::parser_library
namespace hlasm_plugin::parser_library::lexing {
struct u8string_view_with_newlines;
} // namespace hlasm_plugin::parser_library::lexing
namespace hlasm_plugin::parser_library::parsing {
class parser_holder;
} // namespace hlasm_plugin::parser_library::parsing
namespace hlasm_plugin::parser_library::semantics {
class collector;
struct range_provider;
class source_info_processor;
} // namespace hlasm_plugin::parser_library::semantics
namespace hlasm_plugin::parser_library {
struct analyzing_context;
} // namespace hlasm_plugin::parser_library

namespace hlasm_plugin::parser_library::processing {
class processing_manager;

enum class ainsert_destination
{
    back,
    front,
};

struct opencode_provider_options
{
    bool ictl_allowed;
    int process_remaining;
};

enum class extract_next_logical_line_result
{
    failed,
    normal,
    ictl,
    process,
};

// uses the parser implementation to produce statements in the opencode(-like) scenario
class opencode_provider final : public statement_provider, virtual_file_monitor
{
    document m_input_document;
    std::size_t m_next_line_index = 0;

    lexing::logical_line<utils::utf8_iterator<std::string_view::iterator, utils::utf8_utf16_counter>>
        m_current_logical_line;
    struct logical_line_origin
    {
        size_t begin_line;
        size_t first_index;
        size_t last_index;
        enum class source_type
        {
            none,
            file,
            copy,
            ainsert,
        } source;
    } m_current_logical_line_source;

    std::deque<std::string> m_ainsert_buffer;

    std::shared_ptr<std::unordered_map<context::id_index, std::string>> m_virtual_files;

    struct parser_set
    {
        std::unique_ptr<parsing::parser_holder> m_parser;
        std::unique_ptr<parsing::parser_holder> m_lookahead_parser;
        std::unique_ptr<parsing::parser_holder> m_operand_parser;
    };
    parser_set m_parsers;

    analyzing_context m_ctx;
    parse_lib_provider* m_lib_provider;
    processing::processing_state_listener* m_state_listener;
    const processing::processing_manager& m_processing_manager;
    semantics::source_info_processor* m_src_proc;
    diagnosable_ctx* m_diagnoser;

    opencode_provider_options m_opts;

    bool m_line_fed = false;

    std::unique_ptr<preprocessor> m_preprocessor;

    virtual_file_monitor* m_virtual_file_monitor;
    std::vector<std::pair<virtual_file_handle, utils::resource::resource_location>>& m_vf_handles;

    struct op_data
    {
        std::optional<lexing::u8string_with_newlines> op_text;
        range op_range;
        size_t op_logical_column;
    };

    struct process_ordinary_restart_data
    {
        const statement_processor& proc;
        semantics::collector& collector;
        op_data operands;
        diagnostic_op_consumer* diags;
        std::optional<context::id_index> resolved_instr;
    };

    std::optional<process_ordinary_restart_data> m_restart_process_ordinary;

    struct encoding_warning_issued
    {
        bool server = false;
        bool client = false;
    };

    encoding_warning_issued m_encoding_warning_issued = {};

    std::vector<context::id_index> lookahead_references;

    std::pair<virtual_file_handle, std::string_view> file_generated(std::string_view content) override;

public:
    // rewinds position in file
    void rewind_input(context::source_position pos);
    [[nodiscard]] std::variant<std::string, utils::value_task<std::string>> aread();
    void ainsert(const std::string& rec, ainsert_destination dest);

    opencode_provider(std::string_view text,
        const analyzing_context& ctx,
        parse_lib_provider& lib_provider,
        processing::processing_state_listener& state_listener,
        const processing::processing_manager& proc_manager,
        semantics::source_info_processor& src_proc,
        diagnosable_ctx& diag_consumer,
        std::unique_ptr<preprocessor> preprocessor,
        opencode_provider_options opts,
        virtual_file_monitor* virtual_file_monitor,
        std::vector<std::pair<virtual_file_handle, utils::resource::resource_location>>& vf_handles);
    ~opencode_provider();

    parsing::parser_holder& parser(); // for testing only

    context::shared_stmt_ptr get_next(const processing::statement_processor& processor) override;

    bool finished() const override;

    processing::preprocessor* get_preprocessor();

    void onetime_action();

private:
    void feed_line(parsing::parser_holder& p, bool is_process, bool produce_source_info);
    bool is_comment();
    void process_comment();
    void generate_aread_highlighting(std::string_view text, size_t line_no) const;
    bool is_next_line_ictl() const;
    bool is_next_line_process() const;
    void generate_continuation_error_messages(diagnostic_op_consumer* diags) const;
    extract_next_logical_line_result extract_next_logical_line_from_copy_buffer();
    extract_next_logical_line_result extract_next_logical_line();

    parsing::parser_holder& prepare_operand_parser(lexing::u8string_view_with_newlines text,
        context::hlasm_context& hlasm_ctx,
        diagnostic_op_consumer* diag_collector,
        semantics::range_provider range_prov,
        range text_range,
        size_t logical_column,
        const processing_status& proc_status);

    std::shared_ptr<const context::hlasm_statement> process_lookahead(
        const statement_processor& proc, semantics::collector& collector, op_data operands);

    std::shared_ptr<const context::hlasm_statement> process_ordinary(const statement_processor& proc,
        semantics::collector& collector,
        op_data operands,
        diagnostic_op_consumer* diags,
        std::optional<context::id_index> resolved_instr);

    bool should_run_preprocessor() const noexcept;
    [[nodiscard]] utils::task run_preprocessor();
    enum class remove_empty : bool
    {
        no,
        yes,
    };
    bool suspend_copy_processing(remove_empty re) const;
    [[nodiscard]] utils::task convert_ainsert_buffer_to_copybook();

    [[nodiscard]] utils::task start_preprocessor();
    [[nodiscard]] utils::task start_nested_parser(
        std::string_view text, analyzer_options opts, context::id_index vf_name) const;

    std::string aread_from_copybook() const;
    std::string try_aread_from_document();

    [[nodiscard]] utils::value_task<std::string> deferred_aread(utils::task prep_task);
};

} // namespace hlasm_plugin::parser_library::processing
#endif
