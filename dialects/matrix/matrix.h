#ifndef THORIN_DIALECTS_MATRIX_MATRIX_H
#define THORIN_DIALECTS_MATRIX_MATRIX_H

#include <thorin/pass/pipelinebuilder.h>
#include <thorin/world.h>

#include "dialects/matrix/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::matrix {

#define INTERNAL_PREFIX "internal_mapRed_"

/// %mat.zero: Π [n: .Nat, S: «n; .Nat», m: .Nat] -> %mat.Mat (n,S,(.Idx m));
inline const Def* zero_int(World& w, const Def* n, const Def* S, Def* mem, nat_t m) {
    // TODO: use thorin definition by name
    return w.app(w.ax<matrix::constMat>(), {n, S, w.type_idx(m), mem, w.lit_idx(m, 0)});
}

inline const Def* op_read(const Def* mem, const Def* matrix, const Def* idx) {
    auto& world = matrix->world();
    auto mat_ty = match<Mat>(matrix->type());
    if (!mat_ty) return matrix;
    assert(mat_ty);
    world.DLOG("matrix read: {}[{}]", matrix, idx);
    world.DLOG(" matrix type: {}", matrix->type());
    auto [n, S, T] = mat_ty->args<3>();
    world.DLOG(" (n,S,T): {}, {}, {}", n, S, T);
    return world.app(world.app(world.ax<read>(), {n, S, T}), {mem, matrix, idx});
    // assert(0);
    // return w.app(w.ax<matrix::constMat>(), {n, S, w.type_idx(m), mem, w.lit_idx(m, 0)});
}

} // namespace thorin::matrix

#endif
