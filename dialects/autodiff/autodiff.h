#pragma once

#include "thorin/world.h"

#include "dialects/autodiff/autogen.h"

namespace thorin::autodiff {

/// @name Helper Funcitons for Automatic Differentiation
///@{
inline const Def* op_autodiff(const Def* fun) {
    World& world = fun->world();
    // We rely on the normalized thorin convention that all arguments in functions are grouped.
    // `cn[[args], cont:=cn[returns]]`
    return world.app(world.app(world.ax<ad>(), fun->type()), fun);
}

inline const Def* op_zero(const Def* A) {
    World& world = A->world();
    return world.app(world.ax<zero>(), A);
}

const Def* id_pullback(const Def*);
const Def* zero_pullback(const Def* E, const Def* A);

const Def* tangent_type_fun(const Def*);
const Pi* autodiff_type_fun_pi(const Pi*);
const Def* autodiff_type_fun(const Def*);
const Pi* pullback_type(const Def* E, const Def* A);

const Def* zero_def(const Def* T);
const Def* op_sum(const Def* T, DefArray defs);
///@}

} // namespace thorin::autodiff
