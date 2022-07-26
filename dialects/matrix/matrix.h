#ifndef THORIN_DIALECTS_MATRIX_MATRIX_H
#define THORIN_DIALECTS_MATRIX_MATRIX_H

#include "thorin/world.h"

#include "dialects/matrix/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::matrix {

/// %mat.zero: Π [n: .Nat, S: «n; .Nat», m: .Nat] -> %mat.Mat (n,S,(%Int m));
inline const Def* zero_int(World& w, const Def* n, const Def* S, Def* mem, nat_t m) {
    // TODO: use thorin definition by name
    return w.app(w.ax<matrix::constMat>(), {n, S, w.type_int_width(m), mem, w.lit_int_width(0, m)});
}

} // namespace thorin::matrix

#endif
