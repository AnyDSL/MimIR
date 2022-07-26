#pragma once

#include "thorin/world.h"

#include "dialects/direct/autogen.h"

namespace thorin::direct {

const Def* op_cps2ds(const Def* cps) {
    World& w     = cps->world();
    const Pi* ty = cps->type()->as<Pi>();
    return w.app(w.app(w.ax<cps2ds>(), {ty->dom(), ty->codom()}), cps);
}

} // namespace thorin::direct
