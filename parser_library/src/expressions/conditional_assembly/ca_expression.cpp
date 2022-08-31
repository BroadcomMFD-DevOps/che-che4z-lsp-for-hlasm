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

#include "ca_expression.h"

#include "expressions/evaluation_context.h"

namespace hlasm_plugin::parser_library::expressions {

ca_expression::ca_expression(context::SET_t_enum expr_kind, range expr_range)
    : expr_range(std::move(expr_range))
    , expr_kind(expr_kind)
{}

context::SET_t ca_expression::convert_return_types(
    context::SET_t retval, context::SET_t_enum type, const evaluation_context&) const
{
    if (type != retval.type)
    {
        if (retval.type == context::SET_t_enum::A_TYPE && type == context::SET_t_enum::B_TYPE)
            return retval.access_a() != 0;
        if (retval.type == context::SET_t_enum::B_TYPE && type == context::SET_t_enum::A_TYPE)
            return (context::A_t)retval.access_b();

        return context::SET_t(type);
    }
    return retval;
}

template<typename T>
T ca_expression::evaluate(const evaluation_context& eval_ctx) const
{
    static_assert(context::object_traits<T>::type_enum != context::SET_t_enum::UNDEF_TYPE);

    evaluation_context& ca_eval_ctx = const_cast<evaluation_context&>(eval_ctx);
    auto orig_parent_expression = eval_ctx.parent_expression_type;
    ca_eval_ctx.parent_expression_type = context::object_traits<T>::type_enum;

    auto ret = evaluate(ca_eval_ctx);
    ret = convert_return_types(std::move(ret), context::object_traits<T>::type_enum, eval_ctx);

    ca_eval_ctx.parent_expression_type = orig_parent_expression;

    if constexpr (context::object_traits<T>::type_enum == context::SET_t_enum::A_TYPE)
        return ret.access_a();
    if constexpr (context::object_traits<T>::type_enum == context::SET_t_enum::B_TYPE)
        return ret.access_b();
    if constexpr (context::object_traits<T>::type_enum == context::SET_t_enum::C_TYPE)
        return std::move(ret.access_c());
}
template context::A_t ca_expression::evaluate(const evaluation_context& eval_ctx) const;
template context::B_t ca_expression::evaluate(const evaluation_context& eval_ctx) const;
template context::C_t ca_expression::evaluate(const evaluation_context& eval_ctx) const;

} // namespace hlasm_plugin::parser_library::expressions
