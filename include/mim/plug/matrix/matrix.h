#pragma once

#include <mim/world.h>

#include "mim/plug/matrix/autogen.h"

namespace mim::plug::matrix {

static constexpr auto internal_prefix = "internal_mapRed_";

/// %mat.zero: [n: Nat, S: «n; Nat», m: Nat] -> %mat.Mat (n,S,(Idx m));
inline const Def* zero_int(World& w, const Def* n, const Def* S, const Def* mem, nat_t m) {
    // TODO: use mim definition by name
    return w.app(w.annex<matrix::constMat>(), {n, S, w.type_idx(m), mem, w.lit_idx(m, 0)});
}

inline const Def* op_read(const Def* mem, const Def* matrix, const Def* idx) {
    auto& world = matrix->world();
    auto mat_ty = Axm::isa<Mat>(matrix->type());
    if (!mat_ty) return matrix;
    assert(mat_ty);
    world.DLOG("matrix read: {}[{}]", matrix, idx);
    world.DLOG(" matrix type: {}", matrix->type());
    auto [n, S, T] = mat_ty->args<3>();
    world.DLOG(" (n,S,T): {}, {}, {}", n, S, T);
    return world.app(world.app(world.annex<read>(), {n, S, T}), {mem, matrix, idx});
}

} // namespace mim::plug::matrix
