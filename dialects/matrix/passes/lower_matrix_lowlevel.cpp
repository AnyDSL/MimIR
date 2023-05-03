#include "dialects/matrix/passes/lower_matrix_lowlevel.h"

#include <cassert>

#include <iostream>

#include <thorin/lam.h>

#include "thorin/axiom.h"
#include "thorin/def.h"

#include "dialects/affine/affine.h"
#include "dialects/core/autogen.h"
#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/matrix/autogen.h"
#include "dialects/matrix/matrix.h"
#include "dialects/mem/mem.h"

namespace thorin::matrix {

namespace {

Ref op_lea_tuple(Ref arr, Ref tuple) {
    auto& world = arr->world();
    world.DLOG("op_lea_tuple arr {} : {}", arr, arr->type());
    auto n       = tuple->num_projs();
    auto element = arr;
    for (size_t i = 0; i < n; ++i) element = mem::op_lea(element, tuple->proj(n, i));
    return element;
}

Ref op_pack_tuple(u64 n, Ref tuple, Ref val) {
    auto& world = val->world();
    // TODO: find out why num_projs is wrong
    auto element = val;
    for (int i = n - 1; i >= 0; i--) {
        auto dim = tuple->proj(n, i);
        element  = world.pack(dim, element);
    }
    world.DLOG("op_pack_tuple: {} -> {}", val, element);
    world.DLOG("  for tuple: {} : {}", tuple, tuple->type());
    return element;
}

Ref arr_ty_of_matrix_ty(Ref S, Ref T) {
    auto& world = S->world();
    auto n      = S->num_projs();
    auto arr_ty = T;
    for (int i = n - 1; i >= 0; i--) {
        auto dim = S->proj(n, i);
        arr_ty   = world.arr(dim, arr_ty);
    }
    return arr_ty;
}

} // namespace

Ref LowerMatrixLowLevel::rewrite_imm(Ref def) {
    assert(!match<matrix::map_reduce>(def) && "map_reduce should have been lowered to for loops by now");
    assert(!match<matrix::shape>(def) && "high level operations should have been lowered to for loops by now");
    assert(!match<matrix::prod>(def) && "high level operations should have been lowered to for loops by now");
    assert(!match<matrix::transpose>(def) && "high level operations should have been lowered to for loops by now");
    assert(!match<matrix::sum>(def) && "high level operations should have been lowered to for loops by now");

    // TODO: generalize arg rewrite
    if (auto mat_ax = match<matrix::Mat>(def)) {
        auto [_, S, T] = mat_ax->args<3>();
        S              = rewrite(S);
        T              = rewrite(T);
        auto arr_ty    = arr_ty_of_matrix_ty(S, T);

        auto addr_space = world().lit_nat_0();
        auto ptr_ty     = world().call<mem::Ptr>(Defs{arr_ty, addr_space});

        return ptr_ty;
    } else if (auto init_ax = match<matrix::init>(def)) {
        world().DLOG("init {} : {}", def, def->type());
        auto [_, S, T, mem] = init_ax->args<4>();
        world().DLOG("  S T mem {} {} {}", S, T, mem);
        S   = rewrite(S);
        T   = rewrite(T);
        mem = rewrite(mem);
        world().DLOG("  S T mem {} {} {}", S, T, mem);
        auto arr_ty          = arr_ty_of_matrix_ty(S, T);
        auto [mem2, ptr_mat] = mem::op_alloc(arr_ty, mem)->projs<2>();
        auto res             = world().tuple({mem2, ptr_mat});
        world().DLOG("  res {} : {}", res, res->type());
        return res;
    } else if (auto read_ax = match<matrix::read>(def)) {
        auto [mem, mat, idx] = read_ax->args<3>();
        world().DLOG("read_ax: {}", read_ax);
        world().DLOG("  mem: {} : {}", mem, mem->type());
        world().DLOG("  mat: {} : {}", mat, mat->type());
        world().DLOG("  idx: {} : {}", idx, idx->type());
        mem = rewrite(mem);
        mat = rewrite(mat);
        idx = rewrite(idx);
        world().DLOG("rewritten read");
        world().DLOG("  mem: {} : {}", mem, mem->type());
        world().DLOG("  mat: {} : {}", mat, mat->type());
        world().DLOG("  idx: {} : {}", idx, idx->type());
        // TODO: check if mat is already converted
        auto ptr_mat     = mat;
        auto element_ptr = op_lea_tuple(ptr_mat, idx);
        auto [mem2, val] = world().call<mem::load>(Defs{mem, element_ptr})->projs<2>();
        return world().tuple({mem2, val});
    } else if (auto insert_ax = match<matrix::insert>(def)) {
        auto [mem, mat, idx, val] = insert_ax->args<4>();
        world().DLOG("insert_ax: {}", insert_ax);
        world().DLOG("  mem: {} : {}", mem, mem->type());
        world().DLOG("  mat: {} : {}", mat, mat->type());
        world().DLOG("  idx: {} : {}", idx, idx->type());
        world().DLOG("  val: {} : {}", val, val->type());
        mem = rewrite(mem);
        mat = rewrite(mat);
        idx = rewrite(idx);
        val = rewrite(val);
        world().DLOG("rewritten insert");
        world().DLOG("  mem: {} : {}", mem, mem->type());
        world().DLOG("  mat: {} : {}", mat, mat->type());
        world().DLOG("  idx: {} : {}", idx, idx->type());
        world().DLOG("  val: {} : {}", val, val->type());
        auto ptr_mat     = mat;
        auto element_ptr = op_lea_tuple(ptr_mat, idx);
        auto mem2        = world().call<mem::store>(Defs{mem, element_ptr, val});
        return world().tuple({mem2, ptr_mat});
    } else if (auto const_ax = match<matrix::constMat>(def)) {
        auto [mem, val]      = const_ax->args<2>();
        mem                  = rewrite(mem);
        val                  = rewrite(val);
        auto [n_def, S, T]   = const_ax->callee()->as<App>()->args<3>();
        S                    = rewrite(S);
        T                    = rewrite(T);
        auto arr_ty          = arr_ty_of_matrix_ty(S, T);
        auto [mem2, ptr_mat] = mem::op_alloc(arr_ty, mem)->projs<2>();

        // store initial value
        auto n       = n_def->as<Lit>()->get<u64>();
        auto initial = op_pack_tuple(n, S, val);

        auto mem3 = world().call<mem::store>(Defs{mem2, ptr_mat, initial});

        return world().tuple({mem3, ptr_mat});
    }

    // ignore unapplied axioms to avoid spurious type replacements
    if (def->isa<Axiom>()) return def;

    return Rewriter::rewrite_imm(def); // continue recursive rewriting with everything else
}

} // namespace thorin::matrix
