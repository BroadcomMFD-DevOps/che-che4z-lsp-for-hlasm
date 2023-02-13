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

#ifndef PROCESSING_ASM_PROCESSOR_H
#define PROCESSING_ASM_PROCESSOR_H

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

#include "checking/data_definition/data_def_type_base.h"
#include "context/id_storage.h"
#include "low_language_processor.h"

namespace hlasm_plugin::parser_library {
struct analyzing_context;
class diagnosable_ctx;
struct range;
} // namespace hlasm_plugin::parser_library

namespace hlasm_plugin::parser_library::processing {
class branching_provider;
class opencode_provider;
class processing_manager;
class statement_fields_parser;

struct rebuit_statement;
struct resolved_statement;
} // namespace hlasm_plugin::parser_library::processing

namespace hlasm_plugin::parser_library::workspaces {
class parse_lib_provider;
}

namespace hlasm_plugin::parser_library::processing {

// processor of assembler instructions
class asm_processor : public low_language_processor
{
    using process_table_t = std::unordered_map<context::id_index, std::function<void(rebuilt_statement)>>;

    const process_table_t table_;

public:
    asm_processor(analyzing_context ctx,
        branching_provider& branch_provider,
        workspaces::parse_lib_provider& lib_provider,
        statement_fields_parser& parser,
        opencode_provider& open_code,
        const processing_manager& proc_mgr);

    void process(std::shared_ptr<const processing::resolved_statement> stmt) override;
    struct extract_copy_id_result
    {
        context::id_index name;
        range operand;
        range statement;
    };
    static std::optional<extract_copy_id_result> extract_copy_id(
        const semantics::complete_statement& stmt, diagnosable_ctx* diagnoser);
    static bool process_copy(
        context::id_index name, analyzing_context ctx, workspaces::parse_lib_provider& lib_provider);
    static bool common_copy_postprocess(
        bool processed, const extract_copy_id_result& data, analyzing_context ctx, diagnosable_ctx* diagnoser);

private:
    opencode_provider* open_code_;
    process_table_t create_table();

    context::id_index find_sequence_symbol(const rebuilt_statement& stmt);

    void process_sect(const context::section_kind kind, rebuilt_statement stmt);
    void process_LOCTR(rebuilt_statement stmt);
    void process_EQU(rebuilt_statement stmt);
    void process_DC(rebuilt_statement stmt);
    void process_DS(rebuilt_statement stmt);
    void process_COPY(rebuilt_statement stmt);
    void process_EXTRN(rebuilt_statement stmt);
    void process_WXTRN(rebuilt_statement stmt);
    void process_ORG(rebuilt_statement stmt);
    void process_OPSYN(rebuilt_statement stmt);
    void process_AINSERT(rebuilt_statement stmt);
    void process_CCW(rebuilt_statement stmt);
    void process_CNOP(rebuilt_statement stmt);
    void process_START(rebuilt_statement stmt);
    void process_ALIAS(rebuilt_statement stmt);
    void process_END(rebuilt_statement stmt);
    void process_LTORG(rebuilt_statement stmt);
    void process_USING(rebuilt_statement stmt);
    void process_DROP(rebuilt_statement stmt);
    void process_PUSH(rebuilt_statement stmt);
    void process_POP(rebuilt_statement stmt);
    void process_MNOTE(rebuilt_statement stmt);
    void process_CXD(rebuilt_statement stmt);
    void process_TITLE(rebuilt_statement stmt);

    template<checking::data_instr_type instr_type>
    void process_data_instruction(rebuilt_statement stmt);

    enum class external_type
    {
        strong,
        weak,
    };

    void process_external(rebuilt_statement stmt, external_type t);
};

} // namespace hlasm_plugin::parser_library::processing
#endif
