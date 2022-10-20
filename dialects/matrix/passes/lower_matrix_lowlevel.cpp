#include "dialects/matrix/passes/lower_matrix_lowlevel.h"

#include <cassert>

#include <iostream>

#include <thorin/lam.h>

#include "thorin/axiom.h"

#include "dialects/affine/affine.h"
#include "dialects/core/autogen.h"
#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/matrix/autogen.h"
#include "dialects/matrix/matrix.h"
#include "dialects/mem/mem.h"

namespace thorin::matrix {

const Def* LowerMatrixLowLevel::rewrite(const Def* def) {
    if (auto i = rewritten.find(def); i != rewritten.end()) return i->second;
    rewritten[def] = rewrite_(def);
    return rewritten[def];
}

enum NOpKind { add, mul };

const Def* op_nop(const Def* a, const Def* b, NOpKind kind) {
    auto& world = a->world();
    return world.app(world.ax(kind == add ? core::nop::add : core::nop::mul), {a, b});

    // auto I32   = world.type_int(32);
    // auto a_i32 = core::op_bitcast(I32, a);
    // auto b_i32 = core::op_bitcast(I32, b);
    // auto c_i32 = world.app(world.app(world.ax(kind == add ? core::nop::add : core::nop::mul),
    //                                  {world.lit_nat_0(), world.lit_nat(bitwidth2size(32))}),
    //                        {a_i32, b_i32});
    // auto c     = core::op_bitcast(world.type_nat(), c_i32);
    // return c;
}

// const Def* computeSize(const Def* S) {
//     auto& world = S->world();
//     auto n      = S->num_projs();
//     world.DLOG("compute Size of {} ({} dims)", S, n);
//     const Def* size = world.lit_nat_1();
//     for (size_t i = 0; i < n; i++) {
//         auto dim = S->proj(i);
//         // world.DLOG("dim {}: {}", i, dim);
//         // size = world.app(world.ax(core::nop::mul), {size, dim});
//         size = op_nop(size, dim, mul);
//     }

//     // assert(0);
//     // size = world.lit_nat(42);
//     return size;
// }

// const Def* sizeOfMatrix(const Def* Mat) {
//     auto mat_ax = match<matrix::Mat>(Mat);
//     assert(mat_ax && "type must be a matrix");
//     auto [n_def, S, T] = mat_ax->args<3>();
//     return computeSize(S);
// }

const Def* arrTyOfMatrixTy(const Def* S, const Def* T) {
    auto& world = S->world();
    // auto size   = computeSize(S);
    // auto arr_ty = world.arr(size, T);
    auto n      = S->num_projs();
    auto arr_ty = T;
    for (int i = n - 1; i >= 0; i--) {
        auto dim = S->proj(i);
        world.DLOG("dim {}: {}", i, dim);
        arr_ty = world.arr(dim, arr_ty);
        world.DLOG("arr_ty {}..{}: {}", i, n, arr_ty);
    }
    return arr_ty;
}

const Def* arrTyOfMatrixTy(const Def* Mat) {
    auto& world = Mat->world();
    world.DLOG("compute array type of matrix type {}", Mat);
    auto mat_ax = match<matrix::Mat>(Mat);
    assert(mat_ax && "type must be a matrix");
    auto [n_def, S, T] = mat_ax->args<3>();
    return arrTyOfMatrixTy(S, T);
}

const Def* LowerMatrixLowLevel::rewrite_(const Def* def) {
    auto& world = def->world();

    assert(!match<matrix::mapReduce>(def) && "mapReduce should have been lowered to for loops by now");
    assert(!match<matrix::shape>(def) && "high level operations should have been lowered to for loops by now");
    assert(!match<matrix::prod>(def) && "high level operations should have been lowered to for loops by now");
    assert(!match<matrix::transpose>(def) && "high level operations should have been lowered to for loops by now");
    assert(!match<matrix::sum>(def) && "high level operations should have been lowered to for loops by now");

    if (auto mat_ax = match<matrix::Mat>(def)) {
        // auto [n_def, S, T] = mat_ax->args<3>();
        world.DLOG("Lowering Mat {} to Ptr", mat_ax);
        // auto n = (size_t)(n_def->as<Lit>()->get<u64>());

        // const Def* size = world.app(world.ax<core::nop>(core::nop::mul), {S->proj(0), S->proj(1)});
        // const Def* size2 = S->proj(0);
        // world.DLOG("size2: {} : {}", size2, size2->type());
        // auto size = computeSize(S);

        // world.DLOG("size: {} : {}", size, size->type());

        // auto mat_ty = world.app(world.ax<Mat>(), {world.lit_nat_1(), size, T});
        // return mat_ty;

        // TODO: why does replacement not take effect
        // return world.type_nat();

        // auto arr_ty = world.arr(size, T);
        auto arr_ty = arrTyOfMatrixTy(mat_ax);

        auto addr_space = world.lit_nat_0();
        auto ptr_ty     = world.app(world.ax<mem::Ptr>(), {arr_ty, addr_space});

        return ptr_ty;
    } else if (auto init_ax = match<matrix::init>(def)) {
        auto [n, S, T, mem] = init_ax->args<4>();
        auto arr_ty         = arrTyOfMatrixTy(S, T);
        auto addr_space     = world.lit_nat_0();
        auto ptr_mat        = world.app(world.app(world.ax<mem::alloc>(), {arr_ty, addr_space}), mem);
        return ptr_mat;
    } else if (auto read_ax = match<matrix::read>(def)) {
        auto [mem, mat, idx] = read_ax->args<3>();

    } else if (auto insert_ax = match<matrix::insert>(def)) {
    } else if (auto const_ax = match<matrix::constMat>(def)) {
    }

    return def;
}

PassTag* LowerMatrixLowLevel::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::matrix
