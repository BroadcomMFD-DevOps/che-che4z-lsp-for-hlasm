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

#ifndef SEMANTICS_CONCATENATION_H
#define SEMANTICS_CONCATENATION_H

#include <memory>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "context/common_types.h"
#include "context/id_storage.h"
#include "diagnostic_consumer.h"

// this file is a composition of structures that create concat_chain
// concat_chain is used to represent model statement fields

namespace hlasm_plugin::parser_library::expressions {
struct evaluation_context;
}

namespace hlasm_plugin::parser_library::semantics {

struct variable_symbol;
using vs_ptr = std::unique_ptr<variable_symbol>;

enum class concat_type
{
    STR,
    VAR,
    DOT,
    SUB,
    EQU
};

struct concatenation_point;
using concat_chain = std::vector<concatenation_point>;

// concatenation point representing character string
struct char_str_conc
{
    explicit char_str_conc(std::string value, const range& conc_range)
        : value(std::move(value))
        , conc_range(conc_range)
    {}

    std::string value;
    range conc_range;

    std::string evaluate(const expressions::evaluation_context& eval_ctx) const;
    void resolve(diagnostic_op_consumer& diag) const;
};

// concatenation point representing variable symbol
struct var_sym_conc
{
    explicit var_sym_conc(vs_ptr s)
        : symbol(std::move(s))
    {}

    vs_ptr symbol;

    static std::string evaluate(context::SET_t varsym_value);

    std::string evaluate(const expressions::evaluation_context& eval_ctx) const;
    void resolve(diagnostic_op_consumer& diag) const;
};

// concatenation point representing dot
struct dot_conc
{
    std::string evaluate(const expressions::evaluation_context& eval_ctx) const;
    void resolve(diagnostic_op_consumer& diag) const;
};

// concatenation point representing equals sign
struct equals_conc
{
    std::string evaluate(const expressions::evaluation_context& eval_ctx) const;
    void resolve(diagnostic_op_consumer& diag) const;
};

// concatenation point representing macro operand sublist
struct sublist_conc
{
    explicit sublist_conc(std::vector<concat_chain> list)
        : list(std::move(list))
    {}

    std::vector<concat_chain> list;

    std::string evaluate(const expressions::evaluation_context& eval_ctx) const;
    void resolve(diagnostic_op_consumer& diag) const;
};

// helper stuct for character strings that contain variable symbols
// these points of concatenation when formed into array represent character string in a way that is easily concatenated
// when variable symbols are substituted
struct concatenation_point
{
    std::variant<char_str_conc, var_sym_conc, dot_conc, sublist_conc, equals_conc> value;

    // cleans concat_chains of empty strings and badly parsed operands
    static void clear_concat_chain(concat_chain& conc_list);

    static std::string to_string(const concat_chain& chain);
    static std::string to_string(concat_chain::const_iterator begin, concat_chain::const_iterator end);

    static const var_sym_conc* find_var_sym(concat_chain::const_iterator begin, concat_chain::const_iterator end);

    static std::set<context::id_index> get_undefined_attributed_symbols(
        const concat_chain& chain, const expressions::evaluation_context& eval_ctx);

    concatenation_point(char_str_conc v)
        : value(std::move(v))
    {}
    concatenation_point(var_sym_conc v)
        : value(std::move(v))
    {}
    concatenation_point(dot_conc v)
        : value(std::move(v))
    {}
    concatenation_point(sublist_conc v)
        : value(std::move(v))
    {}
    concatenation_point(equals_conc v)
        : value(std::move(v))
    {}

    static std::string evaluate(const concat_chain& chain, const expressions::evaluation_context& eval_ctx);
    static std::string evaluate(concat_chain::const_iterator begin,
        concat_chain::const_iterator end,
        const expressions::evaluation_context& eval_ctx);

    std::string evaluate(const expressions::evaluation_context& eval_ctx) const;
    void resolve(diagnostic_op_consumer& diag) const;
};

template<bool exact, typename... Ts>
struct concat_chain_matcher
{
    bool operator()(const concat_chain& chain) const
    {
        if constexpr (exact)
        {
            if (chain.size() != sizeof...(Ts))
                return false;
        }
        else
        {
            if (chain.size() < sizeof...(Ts))
                return false;
        }
        return []<size_t... ids>(const concat_chain& ch, std::index_sequence<ids...>)
        {
            return (std::holds_alternative<Ts>(ch[ids].value) && ...);
        }
        (chain, std::index_sequence_for<Ts...>());
    }
    bool operator()(concat_chain::const_iterator b, concat_chain::const_iterator e) const
    {
        if constexpr (exact)
        {
            if (std::distance(b, e) != sizeof...(Ts))
                return false;
        }
        else
        {
            if (std::distance(b, e) < sizeof...(Ts))
                return false;
        }
        return []<size_t... ids>(concat_chain::const_iterator it, std::index_sequence<ids...>)
        {
            return (std::holds_alternative<Ts>((it + ids)->value) && ...);
        }
        (b, std::index_sequence_for<Ts...>());
    }
};

template<typename... Ts>
constexpr const concat_chain_matcher<true, Ts...> concat_chain_matches;
template<typename... Ts>
constexpr const concat_chain_matcher<false, Ts...> concat_chain_starts_with;

} // namespace hlasm_plugin::parser_library::semantics

#endif
