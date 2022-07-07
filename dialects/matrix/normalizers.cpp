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
/// - read(transpose m, (i,j)) -> read(m, (j,i)) (TODO: implement)
/// - read(product m1 m2, (i,j)) -> ... (TODO: implement)
const Def* normalize_read(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world       = type->world();
    auto [mat, index] = arg->projs<2>();

    auto mcm = match<constMat>(mat);
    if (mcm) {
        auto v = mcm->arg();
        return v;
    }
    auto mtrans = match<transpose>(mat);
    if (mtrans) {
        // TODO: need easier decomposition and recomposition of args
        auto [i, j]           = index->projs<2>();
        auto [dims, size, ty] = callee->as<App>()->arg()->projs<3>();
        auto [k, l]           = size->projs<2>();
        auto m                = mtrans->arg();
        auto idx              = world.tuple({j, i});
        auto access           = world.tuple({m, idx});
        auto v = world.app(world.app(world.ax<read>(), world.tuple({dims, world.tuple({l, k}), ty})), access, dbg);
        return v;
    }

    return world.raw_app(callee, arg, dbg);
}

/// Normalizer for write operations
/// TODO: implement
const Def* normalize_insert(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world            = type->world();
    auto [mat, index, val] = arg->projs<3>();

    // same as read
    // TODO: eliminate duplicate code
    auto mtrans = match<transpose>(mat);
    if (mtrans) {
        // TODO: need easier decomposition and recomposition of args
        auto [i, j]           = index->projs<2>();
        auto [dims, size, ty] = callee->as<App>()->arg()->projs<3>();
        auto [k, l]           = size->projs<2>();
        auto m                = mtrans->arg();
        auto idx              = world.tuple({j, i});
        auto access           = world.tuple({m, idx, val});
        auto v = world.app(world.app(world.ax<insert>(), world.tuple({dims, world.tuple({l, k}), ty})), access, dbg);
        return v;
    }

    return world.raw_app(callee, arg, dbg);
}

/// Normalizer for transpose operations
/// - transpose (constMat v) -> cosntMat v (TODO: implement)
/// - transpose (insert m v (i,j)) -> insert (transpose m) v (j,i) (TODO: implement, maybe other way around?)
/// - transpose (tranpose m) -> m (TODO: implement)
const Def* normalize_tranpose(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    return world.raw_app(callee, arg, dbg);
}

/// - shape (@mat n (k1,k2,...,kn) i) -> (k1,k2,...,kn)#i (TODO: implement)
const Def* normalize_shape(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world                   = type->world();
    auto [mat, index]             = arg->projs<2>();
    auto [dims, sizes, body_type] = match<Mat, false>(mat->type())->args<3>();

    return world.extract(sizes, index, dbg);
}

/// Matrix normalizer for product on two-dimensional matrices
/// - product (constMat v1, constMat v2) -> constMat v1 * v2 * dim (TODO: implement)
/// - product (constMat v, m) -> ... (TODO: implement)
/// - product (m, constMat v) -> ... (TODO: implement)
/// - product (id, m) -> m
/// - product (m, id) -> m
const Def* normalize_prod(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world        = type->world();
    auto [left, right] = arg->projs<2>();

    auto mleft  = match<id>(left);
    auto mright = match<id>(right);
    if (mleft) { return right; }
    if (mright) { return left; }

    return world.raw_app(callee, arg, dbg);
}

/// - map(constMat v, f) -> constMat f(v) (TODO: implement)
/// - map f (map g m) -> map (f . g) m (TODO: implement)
const Def* normalize_map(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    return world.raw_app(callee, arg, dbg);
}

/// TODO: implement
const Def* normalize_zip(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    return world.raw_app(callee, arg, dbg);
}

/// TODO: implement
const Def* normalize_fold(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    return world.raw_app(callee, arg, dbg);
}

THORIN_matrix_NORMALIZER_IMPL

} // namespace thorin::matrix
