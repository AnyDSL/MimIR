#include "thorin/normalize.h"
#include "thorin/world.h"
#include "thorin/axiom.h"

#include "dialects/matrix.h"

namespace thorin::matrix {

const Def* normalize_read(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world                = type->world();
    return world.lit_int_mod(4294967296,42);
//     auto [mat, index]          = arg->projs<2>();

//     // auto mcm = match<constMat, false>(mat);
//     auto mcm = match<constMat>(mat);
//     // printf("A\n");
//     if(mcm || true) {
//     // printf("B\n");
//     return world.lit_int_mod(4294967296,42);
// //        auto v = cm->arg();
// //        return v;
//     }

//     return world.raw_app(callee, arg, dbg);
}


THORIN_matrix_NORMALIZER_IMPL

} // namespace thorin::matrix
