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

#ifndef PROCESSING_CA_PROCESSOR_H
#define PROCESSING_CA_PROCESSOR_H

#include <optional>

#include "aread_time.h"
#include "instruction_processor.h"
#include "processing/opencode_provider.h"
#include "processing/processing_state_listener.h"
#include "semantics/operand_visitor.h"

namespace hlasm_plugin::parser_library::processing {

// processor of conditional assembly instructions
class ca_processor : public instruction_processor
{
    using process_table_t =
        std::unordered_map<context::id_index, std::function<void(const semantics::complete_statement&)>>;

    const process_table_t table_;
    processing_state_listener& listener_;

public:
    ca_processor(analyzing_context ctx,
        branching_provider& branch_provider,
        workspaces::parse_lib_provider& lib_provider,
        processing_state_listener& listener,
        opencode_provider& open_code);

    void process(std::shared_ptr<const processing::resolved_statement> stmt) override;

private:
    opencode_provider* open_code_;

    process_table_t create_table();

    void register_seq_sym(const semantics::complete_statement& stmt);

    struct SET_info
    {
        context::set_symbol_base* symbol = nullptr;
        context::id_index name;
        context::A_t index = 0;
    };

    struct GLB_LCL_info
    {
        context::id_index id;
        bool scalar;
        range r;

        GLB_LCL_info(context::id_index id, bool scalar, range r)
            : id(id)
            , scalar(scalar)
            , r(r)
        {}
    };
    std::vector<GLB_LCL_info> m_glb_lcl_work;
    std::vector<expressions::ca_expression*> m_set_work;

    template<typename T>
    SET_info get_SET_symbol(const semantics::complete_statement& stmt);
    bool prepare_SET_operands(
        const semantics::complete_statement& stmt, std::vector<expressions::ca_expression*>& expr_values);

    template<typename T>
    void process_SET(const semantics::complete_statement& stmt);

    bool prepare_GBL_LCL(const semantics::complete_statement& stmt, std::vector<GLB_LCL_info>& info) const;

    template<typename T, bool global>
    void process_GBL_LCL(const semantics::complete_statement& stmt);

    void process_ANOP(const semantics::complete_statement& stmt);

    std::optional<context::A_t> prepare_ACTR(const semantics::complete_statement& stmt);
    void process_ACTR(const semantics::complete_statement& stmt);

    const semantics::seq_sym* prepare_AGO(const semantics::complete_statement& stmt);
    void process_AGO(const semantics::complete_statement& stmt);

    const semantics::seq_sym* prepare_AIF(const semantics::complete_statement& stmt);
    void process_AIF(const semantics::complete_statement& stmt);

    void process_MACRO(const semantics::complete_statement& stmt);
    void process_MEXIT(const semantics::complete_statement& stmt);
    void process_MEND(const semantics::complete_statement& stmt);
    void process_AEJECT(const semantics::complete_statement& stmt);
    void process_ASPACE(const semantics::complete_statement& stmt);
    void process_AREAD(const semantics::complete_statement& stmt);

    void process_empty(const semantics::complete_statement&);

    void process_MHELP(const semantics::complete_statement& stmt);
};

} // namespace hlasm_plugin::parser_library::processing

#endif
