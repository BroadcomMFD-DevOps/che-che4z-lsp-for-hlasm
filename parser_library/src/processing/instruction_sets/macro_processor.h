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

#ifndef PROCESSING_MACRO_PROCESSOR_H
#define PROCESSING_MACRO_PROCESSOR_H

#include "context/macro.h"
#include "diagnostic_adder.h"
#include "instruction_processor.h"

namespace hlasm_plugin::parser_library::processing {

// processor of macro instructions
class macro_processor final : public instruction_processor
{
public:
    macro_processor(const analyzing_context& ctx,
        branching_provider& branch_provider,
        parse_lib_provider& lib_provider,
        diagnosable_ctx& diag_ctx);

    void process(std::shared_ptr<const processing::resolved_statement> stmt, const processing_status& status) override;

    static context::macro_data_ptr string_to_macrodata(std::string data, diagnostic_adder& add_diagnostic);

    static context::macro_data_ptr create_macro_data(semantics::concat_chain::const_iterator begin,
        semantics::concat_chain::const_iterator end,
        diagnostic_adder& add_diagnostic);

    auto make_evaluator() const;

private:
    context::macro_data_ptr get_label_args(const resolved_statement& statement) const;
    std::vector<context::macro_arg> get_operand_args(
        const resolved_statement& statement, const processing_status& status) const;
};

} // namespace hlasm_plugin::parser_library::processing
#endif
