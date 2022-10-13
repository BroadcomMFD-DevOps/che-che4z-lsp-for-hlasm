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

#include "ca_operator_binary.h"

#include <limits>

#include "ebcdic_encoding.h"
#include "expressions/conditional_assembly/terms/ca_symbol_attribute.h"
#include "expressions/evaluation_context.h"
#include "semantics/concatenation.h"
#include "semantics/variable_symbol.h"
#include "terms/ca_function.h"
#include "terms/ca_string.h"

namespace hlasm_plugin::parser_library::expressions {

ca_binary_operator::ca_binary_operator(
    ca_expr_ptr left_expr, ca_expr_ptr right_expr, context::SET_t_enum expr_kind, range expr_range)
    : ca_expression(expr_kind, std::move(expr_range))
    , left_expr(std::move(left_expr))
    , right_expr(std::move(right_expr))
{}

undef_sym_set ca_binary_operator::get_undefined_attributed_symbols(const evaluation_context& eval_ctx) const
{
    auto tmp = left_expr->get_undefined_attributed_symbols(eval_ctx);
    tmp.merge(right_expr->get_undefined_attributed_symbols(eval_ctx));
    return tmp;
}

void ca_binary_operator::resolve_expression_tree(ca_expression_ctx expr_ctx, diagnostic_op_consumer& diags)
{
    if (expr_kind != expr_ctx.kind)
        diags.add_diagnostic(diagnostic_op::error_CE004(expr_range));
    else
    {
        left_expr->resolve_expression_tree(expr_ctx, diags);
        right_expr->resolve_expression_tree(expr_ctx, diags);
    }
}

bool ca_binary_operator::is_character_expression(character_expression_purpose purpose) const
{
    return left_expr->is_character_expression(purpose);
}

void ca_binary_operator::apply(ca_expr_visitor& visitor) const
{
    left_expr->apply(visitor);
    right_expr->apply(visitor);
}

context::SET_t ca_binary_operator::evaluate(const evaluation_context& eval_ctx) const
{
    return operation(left_expr->evaluate(eval_ctx), right_expr->evaluate(eval_ctx), eval_ctx);
}

ca_function_binary_operator::ca_function_binary_operator(ca_expr_ptr left_expr,
    ca_expr_ptr right_expr,
    ca_expr_ops function,
    context::SET_t_enum expr_kind,
    range expr_range,
    context::SET_t_enum parent_expr_kind)
    : ca_binary_operator(std::move(left_expr), std::move(right_expr), expr_kind, std::move(expr_range))
    , function(function)
    , m_expr_ctx { expr_kind, parent_expr_kind, true }
{}

std::variant<bool, undef_sym_set> t_attr_special_case(
    const expressions::ca_expression* left, const expressions::ca_expression* right, const evaluation_context& eval_ctx)
{
    const expressions::ca_symbol_attribute* t_attr = nullptr;
    const expressions::ca_string* o_string = nullptr;

    if (t_attr = dynamic_cast<const expressions::ca_symbol_attribute*>(left))
    {
        o_string = dynamic_cast<const expressions::ca_string*>(right);
    }
    else if (t_attr = dynamic_cast<const expressions::ca_symbol_attribute*>(right))
    {
        o_string = dynamic_cast<const expressions::ca_string*>(left);
    }
    if (t_attr == nullptr || o_string == nullptr)
        return false;

    if (t_attr->attribute != context::data_attr_kind::T || !std::holds_alternative<semantics::vs_ptr>(t_attr->symbol)
        || !std::get<semantics::vs_ptr>(t_attr->symbol)->access_basic())
        return false;

    if (auto deps = o_string->get_undefined_attributed_symbols(eval_ctx); !deps.empty())
        return std::move(deps);

    auto v = o_string->evaluate(eval_ctx);

    return v.type == context::SET_t_enum::C_TYPE && v.access_c() == "O";
}

undef_sym_set ca_function_binary_operator::get_undefined_attributed_symbols(const evaluation_context& eval_ctx) const
{
    if (is_relational() && m_expr_ctx.parent_expr_kind == context::SET_t_enum::B_TYPE)
    {
        auto special = t_attr_special_case(left_expr.get(), right_expr.get(), eval_ctx);
        if (std::holds_alternative<undef_sym_set>(special))
            return std::move(std::get<undef_sym_set>(special));
        else if (std::get<bool>(special))
            return undef_sym_set();
    }
    return ca_binary_operator::get_undefined_attributed_symbols(eval_ctx);
}

void ca_function_binary_operator::resolve_expression_tree(ca_expression_ctx expr_ctx, diagnostic_op_consumer& diags)
{
    if (expr_kind != expr_ctx.kind)
        diags.add_diagnostic(diagnostic_op::error_CE004(expr_range));
    else if (!expr_ctx.binary_operators_allowed)
        diags.add_diagnostic(
            diagnostic_op::error_CE005(range(left_expr->expr_range.start, right_expr->expr_range.end)));
    else
    {
        m_expr_ctx = expr_ctx;
        context::SET_t_enum operands_kind;

        if (is_relational())
        {
            // 'A' eq UPPER('a') is ok
            // UPPER('a') eq 'A' is not
            operands_kind = left_expr->is_character_expression(character_expression_purpose::left_side_of_comparison)
                ? context::SET_t_enum::C_TYPE
                : context::SET_t_enum::A_TYPE;
        }
        else
            operands_kind = ca_common_expr_policy::get_operands_type(function, expr_ctx.kind);

        expr_ctx.kind = operands_kind;
        left_expr->resolve_expression_tree(expr_ctx, diags);
        right_expr->resolve_expression_tree(expr_ctx, diags);
    }
}

context::A_t shift_operands(context::A_t lhs, context::A_t rhs, ca_expr_ops shift)
{
    auto shift_part = rhs & 0x3f; // first 6 bits
    if (shift_part == 0)
        return rhs;

    std::uint32_t unsigned_lhs = lhs;
    auto sign_bit = unsigned_lhs & (1U << 31);

    unsigned int result;

    if (shift_part >= 32)
    {
        switch (shift)
        {
            case ca_expr_ops::SLA:
                result = sign_bit;
                break;
            case ca_expr_ops::SRA:
                result = ~0U;
                break;
            default:
                result = 0;
                break;
        }
        return result;
    }

    switch (shift)
    {
        case ca_expr_ops::SLA:
            result = (unsigned_lhs << shift_part) | sign_bit;
            break;
        case ca_expr_ops::SLL:
            result = unsigned_lhs << shift_part;
            break;
        case ca_expr_ops::SRA:
            if (sign_bit)
                result = (unsigned_lhs >> shift_part) | (~0U << (32 - shift_part));
            else
                result = unsigned_lhs >> shift_part;
            break;
        case ca_expr_ops::SRL:
            result = unsigned_lhs >> shift_part;
            break;
        default:
            result = 0;
            break;
    }

    return result;
}

context::SET_t ca_function_binary_operator::operation(
    context::SET_t lhs, context::SET_t rhs, const evaluation_context& eval_ctx) const
{
    if (m_expr_ctx.parent_expr_kind == context::SET_t_enum::A_TYPE)
    {
        switch (function)
        {
            case ca_expr_ops::AND:
                return convert_return_types(lhs.access_a() & rhs.access_a(), expr_kind, eval_ctx);
            case ca_expr_ops::OR:
                return convert_return_types(lhs.access_a() | rhs.access_a(), expr_kind, eval_ctx);
            case ca_expr_ops::XOR:
                return convert_return_types(lhs.access_a() ^ rhs.access_a(), expr_kind, eval_ctx);
            default:
                break;
        }
    }
    else if (m_expr_ctx.parent_expr_kind == context::SET_t_enum::B_TYPE)
    {
        switch (function)
        {
            case ca_expr_ops::AND:
                return convert_return_types(lhs.access_b() && rhs.access_b(), expr_kind, eval_ctx);
            case ca_expr_ops::OR:
                return convert_return_types(lhs.access_b() || rhs.access_b(), expr_kind, eval_ctx);
            case ca_expr_ops::XOR:
                return convert_return_types(lhs.access_b() != rhs.access_b(), expr_kind, eval_ctx);
            default:
                break;
        }
    }

    if (expr_kind == context::SET_t_enum::A_TYPE)
    {
        switch (function)
        {
            case ca_expr_ops::SLA:
            case ca_expr_ops::SLL:
            case ca_expr_ops::SRA:
            case ca_expr_ops::SRL:
                return shift_operands(lhs.access_a(), rhs.access_a(), function);
            case ca_expr_ops::FIND:
                return ca_function::FIND(lhs.access_c(), rhs.access_c());
            case ca_expr_ops::INDEX:
                return ca_function::INDEX(lhs.access_c(), rhs.access_c());
            default:
                break;
        }
    }
    else if (expr_kind == context::SET_t_enum::B_TYPE)
    {
        int comp = 0;
        if (is_relational())
            comp = compare_relational(lhs, rhs, left_expr->expr_kind);

        switch (function)
        {
            case ca_expr_ops::EQ:
                return comp == 0;
            case ca_expr_ops::NE:
                return comp != 0;
            case ca_expr_ops::LE:
                return comp <= 0;
            case ca_expr_ops::LT:
                return comp < 0;
            case ca_expr_ops::GE:
                return comp >= 0;
            case ca_expr_ops::GT:
                return comp > 0;
            default:
                break;
        }
    }
    return context::SET_t(expr_kind);
}

int ca_function_binary_operator::compare_string(const context::C_t& lhs, const context::C_t& rhs)
{
    int diff = (int)lhs.size() - (int)rhs.size();

    if (diff != 0)
        return diff;

    return ebcdic_encoding::to_ebcdic(lhs).compare(ebcdic_encoding::to_ebcdic(rhs));
}

int ca_function_binary_operator::compare_relational(
    const context::SET_t& lhs, const context::SET_t& rhs, context::SET_t_enum type)
{
    switch (type)
    {
        case context::SET_t_enum::A_TYPE:
            return lhs.access_a() - rhs.access_a();
        case context::SET_t_enum::C_TYPE:
            return compare_string(lhs.access_c(), rhs.access_c());
        default:
            return 0;
    }
}

bool ca_function_binary_operator::is_relational() const
{
    switch (function)
    {
        case ca_expr_ops::EQ:
        case ca_expr_ops::NE:
        case ca_expr_ops::LE:
        case ca_expr_ops::LT:
        case ca_expr_ops::GE:
        case ca_expr_ops::GT:
            return true;
        default:
            return false;
    }
}

context::A_t overflow_transform(std::int64_t val, range expr_range, const evaluation_context& eval_ctx)
{
    if (val > std::numeric_limits<context::A_t>::max())
    {
        eval_ctx.diags.add_diagnostic(diagnostic_op::error_CE013(expr_range));
        return 0;
    }
    else if (val < std::numeric_limits<context::A_t>::min())
    {
        eval_ctx.diags.add_diagnostic(diagnostic_op::error_CE014(expr_range));
        return 0;
    }
    else
        return (context::A_t)val;
}

context::SET_t ca_add::operation(
    const context::SET_t& lhs, const context::SET_t& rhs, range expr_range, const evaluation_context& eval_ctx)
{
    return overflow_transform((std::int64_t)lhs.access_a() + (std::int64_t)rhs.access_a(), expr_range, eval_ctx);
}

context::SET_t ca_sub::operation(
    const context::SET_t& lhs, const context::SET_t& rhs, range expr_range, const evaluation_context& eval_ctx)
{
    return overflow_transform((std::int64_t)lhs.access_a() - (std::int64_t)rhs.access_a(), expr_range, eval_ctx);
}

context::SET_t ca_mul::operation(
    const context::SET_t& lhs, const context::SET_t& rhs, range expr_range, const evaluation_context& eval_ctx)
{
    return overflow_transform((std::int64_t)lhs.access_a() * (std::int64_t)rhs.access_a(), expr_range, eval_ctx);
}

context::SET_t ca_div::operation(
    const context::SET_t& lhs, const context::SET_t& rhs, range expr_range, const evaluation_context& eval_ctx)
{
    if (rhs.access_a() == 0)
        return 0;
    return overflow_transform((std::int64_t)lhs.access_a() / (std::int64_t)rhs.access_a(), expr_range, eval_ctx);
}

context::SET_t ca_conc::operation(
    context::SET_t lhs, context::SET_t rhs, range expr_range, const evaluation_context& eval_ctx)
{
    if (lhs.access_c().size() + rhs.access_c().size() > ca_string::MAX_STR_SIZE)
    {
        eval_ctx.diags.add_diagnostic(diagnostic_op::error_CE011(expr_range));
        return context::object_traits<context::C_t>::default_v();
    }
    auto& ret = lhs.access_c();
    ret.reserve(ret.size() + rhs.access_c().size());
    ret.append(rhs.access_c().begin(), rhs.access_c().end());
    return lhs;
}

} // namespace hlasm_plugin::parser_library::expressions
