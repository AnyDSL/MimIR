#pragma once

#include "mim/world.h"

#include "mim/plug/affine/autogen.h"

namespace mim::plug::affine {

/// @name %%affine.For
/// Returns the affine_for axiom applied with \a params.
/// See documentation for %affine.For axiom in @ref affine.
///@{
inline const Def* fn_for(World& w, Defs params) {
    return w.app(w.annex<affine::For>(), {w.lit_i32(), w.lit_nat(params.size()), w.tuple(params)});
}

/// Returns a fully applied affine_for axiom.
/// See documentation for %affine.For axiom in @ref affine.
inline const Def*
op_for(World& w, const Def* begin, const Def* end, const Def* step, Defs inits, const Def* body, const Def* brk) {
    auto types = DefVec(inits.size(), [&](size_t i) { return inits[i]->type(); });
    return w.app(fn_for(w, types), {begin, end, step, w.tuple(inits), body, brk});
}
///@}

} // namespace mim::plug::affine
