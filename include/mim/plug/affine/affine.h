#pragma once

#include "mim/world.h"

#include "mim/plug/affine/autogen.h"

namespace mim::plug::affine {

/// Returns a fully applied affine_for axm.
/// See documentation for %affine.For axm in @ref affine.
inline const Def*
op_for(World& w, const Def* begin, const Def* end, const Def* step, Defs inits, const Def* body, const Def* brk) {
    return w.app(w.annex<affine::For>(), {begin, end, step, w.tuple(inits), body, brk});
}
///@}

} // namespace mim::plug::affine
