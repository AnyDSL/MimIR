#include <iostream>

#include "thorin/axiom.h"
#include "thorin/normalize.h"
#include "thorin/world.h"

#include "dialects/matrix/matrix.h"
namespace thorin::matrix {

/// Normalizer for read opertions
/// - read(constMat v) -> v
/// - read(insert m v i, i) -> v (TODO: check with mapReduce)
/// - read(insert m v i, j) -> read(m, i) if i <> j (TODO: wanted? useful?)
/// - read(transpose m, (i,j)) -> read(m, (j,i)) (TODO: check for mapReduce)
/// - read(product m1 m2, (i,j)) -> ... (TODO: check with mapReduce)
/// - read (mapReduce f) idx = loop f idx (TODO: implement => use inner loop from lowering phase)
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
    auto& world = type->world();
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

auto get_max_index(auto init, auto inputs) {
    auto max_idx = init;

    for (auto inp : inputs) {
        auto [indices, mat] = inp->projs<2>();
        auto indice_count   = isa_lit(indices->arity());
        if (!indice_count) return def;
        for (auto idx : indices->projs()) {
            auto idx_val = isa_lit(idx);
            if (!idx_val) return def;
            if (idx_val > max_idx) max_idx = idx_val;
        }
    }

    return max_idx;
}

/// mapReduce normalizers
/// - mapReduce (..., ((idx,mapReduce([out, ]...), ...))) -> unify idx, out (out is implicit), name vars apart
///   requires: same reduction, distributive reduction
/// we assume distributivity of the reduction function
const Def* normalize_mapReduce(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world                  = type->world();
    auto [zero, add, mul, input] = arg->projs<4>();
    // auto [dims, sizes, body_type] = match<Mat, false>(mat->type())->args<3>();

    auto [n, S, T, m, NI, TI, SI] = callee->as<App>()->args<7>();

    auto def = world.raw_app(callee, arg, dbg);

    auto m_lit = isa_lit(m);
    auto n_lit = isa_lit(n);
    if (!m_lit || !n_lit) return def;

    // get largest used index to name apart
    auto inputs  = input->projs();
    auto max_idx = get_max_index(n_lit, inputs);

    for (auto inp : inputs) {
        auto [idx, mat] = inp->projs<2>();
        //
        auto mapRedMat = match<mapReduce>(mat);
        if (!mapRedMat) continue;
        auto [izero, iadd, imul, iinput]    = mapRedMat->args<4>();
        auto [in, iS, iT, im, iNI, iTI, SI] = mapRedMat->callee()->as<App>()->args<7>();
        // TODO: allow if one of them is useless (dummyAddition)
        if (iadd != add) continue;

        auto in_lit = isa_lit(in);
        if (!isa_lit(iinput->arity())) continue;
        if (!in_lit) continue;
        auto iinputs   = iinput->projs();
        auto inner_max = get_max_index(as_lit(in), iinputs);
        // replace out with idx, add max_idx to others (to avoid name clash)
        // out = (0,1,...,in)
        // => replace i<in with idx[i]
        //    and i>=in with i+max_idx

        bool canReplace = true;
        for (auto iinp : iinputs) {
            auto [iindices, imat] = iinp->projs<2>();
            if (!isa_lit(iindices->arity())) {
                canReplace = false;
                break;
            }
            auto iidxs = iindices->projs();
            for (auto iidx : iidxs) {
                auto iidx_val = isa_lit(iidx);
                if (!iidx_val) {
                    canReplace = false;
                    break;
                }
                nat_t new_idx;
                if (iidx_val < in_lit) {
                    new_idx = ;
                } else {
                    new_idx = iidx_val + max_idx;
                }
            }
        }
        if (!canReplace) continue;

        // increase max_idx with the newly used indices (or something larger)
        max_idx += inner_max;
    }

    // auto n = input->num_projs();

    // auto [zero, add, mul, input] =
    //     mapReduce_ax->args<4>({world.dbg("zero"), world.dbg("add"), world.dbg("mul"), world.dbg("input")});
    // auto inner_callee = mapReduce_ax->callee()->as<App>();
    // auto [n, S, T, m, NI, TI, SI] =
    //     inner_callee->args<7>({world.dbg("n"), world.dbg("S"), world.dbg("T"), world.dbg("m"), world.dbg("NI"),
    //                            world.dbg("TI"), world.dbg("SI")});
}

THORIN_matrix_NORMALIZER_IMPL

} // namespace thorin::matrix
