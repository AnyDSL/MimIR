#ifndef THORIN_DIALECTS_MATRIX_MATRIX_H
#define THORIN_DIALECTS_MATRIX_MATRIX_H

#include "thorin/world.h"

#include "dialects/matrix/autogen.h"

namespace thorin::matrix {

/// %mat.zero: Π [n: .Nat, S: «n; .Nat», m: .Nat] -> %mat.Mat (n,S,(%Int m));
inline const Def* zero(World& w, const Def* n, const Def* S, nat_t m) {
    return w.app(w.ax<matrix::constMat>(), {n, S, w.type_int_width(m), w.lit_int_width(0, m)});
}

} // namespace thorin::matrix

#endif
