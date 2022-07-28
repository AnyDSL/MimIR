#pragma once

#include "thorin/world.h"

#include "dialects/autodiff/autogen.h"

namespace thorin::autodiff {

const Def* op_autodiff(const Def* fun) {
    World& world = fun->world();
    // we rely on the normalized thorin convention that all arguments in functions are grouped
    // cn[[args], cont?=cn[returns]]
    auto [dom, codom] = fun->type()->as<Pi>()->doms<2>();
    // TODO: do we need mem special casing?
    return world.app(world.app(world.ax<autodiff>(), {dom, codom}), fun);
}

} // namespace thorin::autodiff
