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

#include "macro_processor.h"

#include <functional>
#include <memory>
#include <stack>

#include "ebcdic_encoding.h"
#include "parsing/parser_impl.h"
#include "semantics/operand_impls.h"

namespace hlasm_plugin::parser_library::processing {

macro_processor::macro_processor(
    analyzing_context ctx, branching_provider& branch_provider, workspaces::parse_lib_provider& lib_provider)
    : instruction_processor(std::move(ctx), branch_provider, lib_provider)
{}

void macro_processor::process(std::shared_ptr<const processing::resolved_statement> stmt)
{
    const auto next_sysndx = hlasm_ctx.next_sysndx();
    const auto sysndx_limit = hlasm_ctx.sysndx_limit();
    if (next_sysndx > sysndx_limit)
    {
        add_diagnostic(diagnostic_op::error_E072(stmt->stmt_range_ref()));
        return;
    }

    if (const auto& label = stmt->label_ref(); label.type == semantics::label_si_type::ORD)
    {
        auto [valid, id] = hlasm_ctx.try_get_symbol_name(*std::get<semantics::ord_symbol_string>(label.value).symbol);
        if (valid && !hlasm_ctx.ord_ctx.get_symbol(id))
        {
            hlasm_ctx.ord_ctx.add_symbol_reference(
                context::symbol(id,
                    context::symbol_value(),
                    context::symbol_attributes(context::symbol_origin::MACH, 'M'_ebcdic),
                    location(),
                    {}),
                lib_info);
        }
    }

    auto args = get_args(*stmt);
    hlasm_ctx.enter_macro(stmt->opcode_ref().value, std::move(args.name_param), std::move(args.symbolic_params));
}
namespace {
bool is_valid_string(std::string_view s)
{
    size_t index = 0;
    size_t apostrophes = 0;

    while (true)
    {
        auto ap_id = s.find_first_of('\'', index);
        if (ap_id == std::string_view::npos)
            break;

        if (ap_id > 0 && ap_id + 1 < s.size() && parsing::parser_impl::is_attribute_consuming(s[ap_id - 1])
            && parsing::parser_impl::can_attribute_consume(s[ap_id + 1]))
        {
            index = ap_id + 1;
            continue;
        }

        index = ap_id + 1;
        apostrophes++;
    }

    return apostrophes % 2 == 0;
}
} // namespace

std::pair<std::unique_ptr<context::macro_param_data_single>, bool> find_single_macro_param(
    std::string_view data, size_t& start)
{
    // always called in nested configuration
    size_t begin = start;

    while (true)
    {
        start = data.find_first_of(",'()", start);

        if (start == std::string_view::npos)
            return { nullptr, false };

        if (data[start] == '(')
        {
            size_t nest = 1;
            while (nest != 0)
            {
                ++start;
                if (start == data.size())
                    return { nullptr, false };

                if (data[start] == '(')
                    ++nest;
                if (data[start] == ')')
                    --nest;
            }
            ++start;
        }
        else if (data[start] == '\'')
        {
            if (start > 0 && start + 1 < data.size() && parsing::parser_impl::is_attribute_consuming(data[start - 1])
                && parsing::parser_impl::can_attribute_consume(data[start + 1]))
            {
                ++start;
                continue;
            }

            start = data.find_first_of('\'', start + 1);

            if (start == std::string_view::npos)
                return { nullptr, false };

            ++start;
        }
        else
            break;
    }

    auto tmp_start = start;
    bool comma_encountered = data[start] == ',';
    if (comma_encountered)
        ++start;

    return { std::make_unique<context::macro_param_data_single>(std::string(data.substr(begin, tmp_start - begin))),
        comma_encountered };
}

context::macro_data_ptr macro_processor::string_to_macrodata(std::string data)
{
    if (data.empty())
        return std::make_unique<context::macro_param_data_dummy>();

    if (data.front() != '(' || data.back() != ')')
        return std::make_unique<context::macro_param_data_single>(std::move(data));


    std::stack<size_t> nests;
    std::stack<std::vector<context::macro_data_ptr>> macro_data;

    nests.push(0);
    macro_data.emplace();
    bool empty_op_pending = false;

    while (true)
    {
        auto begin = nests.top();

        if (begin == data.size())
            return std::make_unique<context::macro_param_data_single>(std::move(data));

        if (data[begin] == '(')
        {
            nests.push(begin + 1);
            macro_data.emplace();
            empty_op_pending = false;
        }
        else if (data[begin] == ')')
        {
            ++begin;
            nests.pop();

            auto vec = std::move(macro_data.top());
            macro_data.pop();

            if (std::exchange(empty_op_pending, false))
                vec.push_back(std::make_unique<context::macro_param_data_single>(""));

            if (begin != data.size() && data[begin] != ',' && data[begin] != ')')
            {
                auto [tmp_single, comma] = find_single_macro_param(data, begin);
                empty_op_pending = comma;

                if (tmp_single == nullptr)
                    return std::make_unique<context::macro_param_data_single>(std::move(data));

                auto single = context::macro_param_data_composite(std::move(vec)).get_value() + tmp_single->get_value();

                macro_data.top().emplace_back(std::make_unique<context::macro_param_data_single>(std::move(single)));
            }
            else
                macro_data.top().emplace_back(std::make_unique<context::macro_param_data_composite>(std::move(vec)));

            nests.top() = begin + (begin != data.size() && data[begin] == ',' ? 1 : 0);

            if (nests.size() == 1)
            {
                break;
            }
        }
        else
        {
            auto [op, comma] = find_single_macro_param(data, begin);
            macro_data.top().push_back(std::move(op));
            nests.top() = begin;
            empty_op_pending = comma;

            if (macro_data.top().back() == nullptr)
                return std::make_unique<context::macro_param_data_single>(std::move(data));
        }
    }

    if (nests.top() != data.size())
        return std::make_unique<context::macro_param_data_single>(std::string(data));

    assert(macro_data.size() == 1 && macro_data.top().size() == 1);

    return std::move(macro_data.top().front());
}

macro_arguments macro_processor::get_args(const resolved_statement& statement) const
{
    macro_arguments args;

    args.name_param = get_label_args(statement);

    args.symbolic_params = get_operand_args(statement);

    return args;
}

context::macro_data_ptr macro_processor::get_label_args(const resolved_statement& statement) const
{
    const auto& label = statement.label_ref();
    switch (label.type)
    {
        case semantics::label_si_type::CONC:
            return std::make_unique<context::macro_param_data_single>(
                semantics::concatenation_point::evaluate(std::get<semantics::concat_chain>(label.value), eval_ctx));
        case semantics::label_si_type::ORD:
            return std::make_unique<context::macro_param_data_single>(
                std::get<semantics::ord_symbol_string>(label.value).mixed_case);
        case semantics::label_si_type::MAC:
            return std::make_unique<context::macro_param_data_single>(std::get<std::string>(label.value));
        case semantics::label_si_type::VAR:
            return std::make_unique<context::macro_param_data_single>(
                semantics::var_sym_conc::evaluate(std::get<semantics::vs_ptr>(label.value)->evaluate(eval_ctx)));
        default:
            return context::macro_data_ptr();
    }
}

bool is_keyword(const semantics::concat_chain& chain, context::hlasm_context& hlasm_ctx)
{
    return chain.size() >= 2 && chain[0].type() == semantics::concat_type::STR
        && chain[1].type() == semantics::concat_type::EQU
        && hlasm_ctx.try_get_symbol_name(std::get<semantics::char_str_conc>(chain[0].value).value).first;
}

bool can_chain_be_forwarded(const semantics::concat_chain& chain)
{
    if (chain.size() == 1 && chain.front().type() == semantics::concat_type::VAR) // single variable symbol &VAR
        return true;
    if (chain.size() == 2 && chain.front().type() == semantics::concat_type::VAR
        && chain.back().type() == semantics::concat_type::DOT) // single variable symbol with dot &VAR.
        return true;
    return false;
}

std::vector<context::macro_arg> macro_processor::get_operand_args(const resolved_statement& statement) const
{
    std::vector<context::macro_arg> args;
    std::vector<context::id_index> keyword_params;

    for (const auto& op : statement.operands_ref().value)
    {
        if (op->type == semantics::operand_type::EMPTY)
        {
            args.push_back({ std::make_unique<context::macro_param_data_dummy>(), nullptr });
            continue;
        }

        auto tmp = op->access_mac()->access_chain();
        assert(tmp);

        auto& tmp_chain = tmp->chain;

        if (is_keyword(tmp_chain, hlasm_ctx)) // keyword
        {
            get_keyword_arg(statement, tmp_chain, args, keyword_params, tmp->operand_range);
        }
        else if (can_chain_be_forwarded(tmp_chain)) // single varsym
        {
            context::macro_data_ptr data = string_to_macrodata(semantics::var_sym_conc::evaluate(
                std::get<semantics::var_sym_conc>(tmp_chain.front().value).symbol->evaluate(eval_ctx)));

            args.push_back({ std::move(data), nullptr });
        }
        else // rest
        {
            diagnostic_adder add_diags(this->eval_ctx.diags, statement.operands_ref().field_range);
            args.push_back({ create_macro_data(tmp_chain.begin(), tmp_chain.end(), eval_ctx, add_diags), nullptr });
        }
    }

    return args;
}

void macro_processor::get_keyword_arg(const resolved_statement& statement,
    const semantics::concat_chain& chain,
    std::vector<context::macro_arg>& args,
    std::vector<context::id_index>& keyword_params,
    range op_range) const
{
    auto id = hlasm_ctx.try_get_symbol_name(std::get<semantics::char_str_conc>(chain[0].value).value).second;
    assert(id != context::id_storage::empty_id);

    auto named = hlasm_ctx.get_macro_definition(statement.opcode_ref().value)->named_params().find(id);
    if (named == hlasm_ctx.get_macro_definition(statement.opcode_ref().value)->named_params().end()
        || named->second->param_type == context::macro_param_type::POS_PAR_TYPE)
    {
        add_diagnostic(diagnostic_op::warning_W014(op_range));

        // MACROCASE TODO
        auto name = std::get<semantics::char_str_conc>(chain[0].value).value;

        args.push_back({ std::make_unique<context::macro_param_data_single>(
                             name + "=" + semantics::concatenation_point::to_string(chain.begin() + 2, chain.end())),
            nullptr });
    }
    else
    {
        if (std::find(keyword_params.begin(), keyword_params.end(), id) != keyword_params.end())
            add_diagnostic(diagnostic_op::error_E011("Keyword", op_range));
        else
            keyword_params.push_back(id);

        auto chain_begin = chain.begin() + 2;
        auto chain_end = chain.end();
        auto chain_size = chain.size() - 2;
        context::macro_data_ptr data;

        if (chain_size == 1 && (*chain_begin).type() == semantics::concat_type::SUB)
        {
            diagnostic_adder add_diags(statement.operands_ref().field_range);
            data = create_macro_data(chain_begin, chain_end, eval_ctx, add_diags);
        }
        else
            data = string_to_macrodata(semantics::concatenation_point::evaluate(chain_begin, chain_end, eval_ctx));

        args.push_back({ std::move(data), id });
    }
}

context::macro_data_ptr create_macro_data_inner(semantics::concat_chain::const_iterator begin,
    semantics::concat_chain::const_iterator end,
    const std::function<std::string(semantics::concat_chain::const_iterator, semantics::concat_chain::const_iterator)>&
        to_string,
    diagnostic_adder& add_diagnostic,
    bool nested = false)
{
    auto size = end - begin;
    if (size == 0)
        return std::make_unique<context::macro_param_data_dummy>();
    else if (size == 1 && begin->type() != semantics::concat_type::SUB)
        return macro_processor::string_to_macrodata(to_string(begin, end));
    else if (size > 1)
    {
        if (auto s = to_string(begin, end);
            s.front() != '(' && (!nested || semantics::concatenation_point::find_var_sym(begin, end) == nullptr))
            return macro_processor::string_to_macrodata(std::move(s));
        else if (is_valid_string(s))
            return macro_processor::string_to_macrodata(std::move(s));
        else
        {
            add_diagnostic(diagnostic_op::error_S0005);
            return std::make_unique<context::macro_param_data_dummy>();
        }
    }

    const auto& inner_chains = std::get<semantics::sublist_conc>(begin->value).list;

    std::vector<context::macro_data_ptr> sublist;

    for (auto& inner_chain : inner_chains)
    {
        sublist.push_back(
            create_macro_data_inner(inner_chain.begin(), inner_chain.end(), to_string, add_diagnostic, true));
    }
    return std::make_unique<context::macro_param_data_composite>(std::move(sublist));
}

context::macro_data_ptr macro_processor::create_macro_data(semantics::concat_chain::const_iterator begin,
    semantics::concat_chain::const_iterator end,
    diagnostic_adder& add_diagnostic)
{
    auto tmp = semantics::concatenation_point::find_var_sym(begin, end);
    if (tmp)
    {
        add_diagnostic(diagnostic_op::error_E064);
        return std::make_unique<context::macro_param_data_dummy>();
    }

    auto f = [](semantics::concat_chain::const_iterator b, semantics::concat_chain::const_iterator e) {
        return semantics::concatenation_point::to_string(b, e);
    };
    return create_macro_data_inner(begin, end, f, add_diagnostic);
}

context::macro_data_ptr macro_processor::create_macro_data(semantics::concat_chain::const_iterator begin,
    semantics::concat_chain::const_iterator end,
    const expressions::evaluation_context& eval_ctx,
    diagnostic_adder& add_diagnostic)
{
    auto f = [&eval_ctx](semantics::concat_chain::const_iterator b, semantics::concat_chain::const_iterator e) {
        return semantics::concatenation_point::evaluate(b, e, eval_ctx);
    };
    return create_macro_data_inner(begin, end, f, add_diagnostic);
}

} // namespace hlasm_plugin::parser_library::processing
