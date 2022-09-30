#pragma once

#include "thorin/world.h"

#include "dialects/autodiff/autogen.h"

namespace thorin::autodiff {

/*

See the paper [TODO] for a description of the autodiff dialect.

*/

// inline is important
// otherwise, the compilation creates two definitions of the functions (irregardless of include guards)
inline const Def* op_autodiff(const Def* fun) {
    World& world = fun->world();
    // we rely on the normalized thorin convention that all arguments in functions are grouped
    // cn[[args], cont?=cn[returns]]
    // TODO: do we need mem special casing?
    // auto [dom, codom] = fun->type()->as<Pi>()->doms<2>();
    // return world.app(world.app(world.ax<autodiff>(), {dom, codom}), fun);
    return world.app(world.app(world.ax<autodiff>(), fun->type()), fun);
}

inline const Def* op_zero(const Def* A) {
    World& world = A->world();
    return world.app(world.ax<zero>(), A);
}

} // namespace thorin::autodiff