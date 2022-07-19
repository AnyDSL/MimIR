#include <iostream>

#include "thorin/axiom.h"
#include "thorin/normalize.h"
#include "thorin/world.h"

#include "dialects/matrix/matrix.h"
namespace thorin::matrix {

/// Normalizer for read opertions
/// - read(constMat v) -> v
/// - read(insert m v i, i) -> v (TODO: implement)
/// - read(insert m v i, j) -> read(m, i) if i <> j (TODO: wanted? useful?)
/// - read(transpose m, (i,j)) -> read(m, (j,i)) (TODO: check for mapReduce)
/// - read(product m1 m2, (i,j)) -> ... (TODO: implement)
/// - read (mapReduce f) idx = f idx
const Def* normalize_read(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world       = type->world();
    auto [mat, index] = arg->projs<2>();

    auto mcm = match<constMat>(mat);
    if (mcm) {
        auto v = mcm->arg();
        return v;
    }

    return world.raw_app(callee, arg, dbg);
}

/// Normalizer for write operations
/// TODO: implement
const Def* normalize_insert(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world            = type->world();
    // auto [mat, index, val] = arg->projs<3>();

    // same as read
    // TODO:

    return world.raw_app(callee, arg, dbg);
}

/// Normalizer for transpose operations
/// - transpose (constMat v) -> cosntMat v (TODO: implement)
/// - transpose (insert m v (i,j)) -> insert (transpose m) v (j,i) (TODO: implement, maybe other way around?)
/// - transpose (tranpose m) -> m (TODO: implement)

/// - shape (@mat n (k1,k2,...,kn) i) -> (k1,k2,...,kn)#i (TODO: implement)
const Def* normalize_shape(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world                   = type->world();
    auto [mat, index]             = arg->projs<2>();
    auto [dims, sizes, body_type] = match<Mat, false>(mat->type())->args<3>();
    (void)callee;

    return world.extract(sizes, index, dbg);
}

/// Matrix normalizer for product on two-dimensional matrices
/// - product (constMat v1, constMat v2) -> constMat v1 * v2 * dim (TODO: implement)
/// - product (constMat v, m) -> ... (TODO: implement)
/// - product (m, constMat v) -> ... (TODO: implement)
/// - product (id, m) -> m (TODO: check)
/// - product (m, id) -> m

/// - map(constMat v, f) -> constMat f(v) (TODO: implement)
/// - map f (map g m) -> map (f . g) m (TODO: implement)
/// - map f (zipWith g m1 m2) -> zipWith (f . g) m1 m2 (TODO: implement)

/// TODO: implement

/// TODO: implement

THORIN_matrix_NORMALIZER_IMPL

} // namespace thorin::matrix
