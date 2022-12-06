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

const Def* op_lea_tuple(const Def* arr, const Def* tuple) {
    // mem::op_lea(arr, tuple);
    auto& world = arr->world();
    world.DLOG("op_lea_tuple arr {} : {}", arr, arr->type());
    auto n       = tuple->num_projs();
    auto element = arr;
    for (size_t i = 0; i < n; ++i) { element = mem::op_lea(element, tuple->proj(n, i)); }
    return element;
}

const Def* op_pack_tuple(u64 n, const Def* tuple, const Def* val) {
    auto& world = val->world();
    // TODO: find out why num_projs is wrong
    // auto n = val->num_projs();
    // world.DLOG("create {} dimensional pack", n);
    auto element = val;
    for (int i = n - 1; i >= 0; i--) {
        auto dim = tuple->proj(n, i);
        // world.DLOG("dim {}: {}", i, dim);
        element = world.pack(dim, element);
    }
    world.DLOG("op_pack_tuple: {} -> {}", val, element);
    world.DLOG("  for tuple: {} : {}", tuple, tuple->type());
    return element;
}

const Def* arrTyOfMatrixTy(const Def* S, const Def* T) {
    auto& world = S->world();
    // auto size   = computeSize(S);
    // auto arr_ty = world.arr(size, T);
    auto n      = S->num_projs();
    auto arr_ty = T;
    for (int i = n - 1; i >= 0; i--) {
        auto dim = S->proj(n, i);
        // world.DLOG("dim {}: {}", i, dim);
        arr_ty = world.arr(dim, arr_ty);
        // world.DLOG("arr_ty {}..{}: {}", i, n, arr_ty);
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

const Def* LowerMatrixLowLevel::rewrite_structural(const Def* def) {
    auto& world = def->world();

    assert(!match<matrix::mapReduce>(def) && "mapReduce should have been lowered to for loops by now");
    assert(!match<matrix::shape>(def) && "high level operations should have been lowered to for loops by now");
    assert(!match<matrix::prod>(def) && "high level operations should have been lowered to for loops by now");
    assert(!match<matrix::transpose>(def) && "high level operations should have been lowered to for loops by now");
    assert(!match<matrix::sum>(def) && "high level operations should have been lowered to for loops by now");

    // TODO: generalize arg rewrite
    if (auto mat_ax = match<matrix::Mat>(def)) {
        auto [_, S, T] = mat_ax->args<3>();
        S              = rewrite(S);
        T              = rewrite(T);
        auto arr_ty    = arrTyOfMatrixTy(S, T);

        auto addr_space = world.lit_nat_0();
        auto ptr_ty     = world.app(world.ax<mem::Ptr>(), {arr_ty, addr_space});

        return ptr_ty;
    } else if (auto init_ax = match<matrix::init>(def)) {
        world.DLOG("init {} : {}", def, def->type());
        auto [_, S, T, mem] = init_ax->args<4>();
        world.DLOG("  S T mem {} {} {}", S, T, mem);
        S   = rewrite(S);
        T   = rewrite(T);
        mem = rewrite(mem);
        world.DLOG("  S T mem {} {} {}", S, T, mem);
        auto arr_ty          = arrTyOfMatrixTy(S, T);
        auto [mem2, ptr_mat] = mem::op_alloc(arr_ty, mem)->projs<2>();
        auto res             = world.tuple({mem2, ptr_mat});
        world.DLOG("  res {} : {}", res, res->type());
        return res;
    } else if (auto read_ax = match<matrix::read>(def)) {
        auto [mem, mat, idx] = read_ax->args<3>();
        world.DLOG("read_ax: {}", read_ax);
        world.DLOG("  mem: {} : {}", mem, mem->type());
        world.DLOG("  mat: {} : {}", mat, mat->type());
        world.DLOG("  idx: {} : {}", idx, idx->type());
        mem = rewrite(mem);
        mat = rewrite(mat);
        idx = rewrite(idx);
        world.DLOG("rewritten read");
        world.DLOG("  mem: {} : {}", mem, mem->type());
        world.DLOG("  mat: {} : {}", mat, mat->type());
        world.DLOG("  idx: {} : {}", idx, idx->type());
        // TODO: check if mat is already converted
        auto ptr_mat     = mat;
        auto element_ptr = op_lea_tuple(ptr_mat, idx);
        auto [mem2, val] = mem::op_load(mem, element_ptr)->projs<2>();
        return world.tuple({mem2, val});
    } else if (auto insert_ax = match<matrix::insert>(def)) {
        auto [mem, mat, idx, val] = insert_ax->args<4>();
        world.DLOG("insert_ax: {}", insert_ax);
        world.DLOG("  mem: {} : {}", mem, mem->type());
        world.DLOG("  mat: {} : {}", mat, mat->type());
        world.DLOG("  idx: {} : {}", idx, idx->type());
        world.DLOG("  val: {} : {}", val, val->type());
        mem = rewrite(mem);
        mat = rewrite(mat);
        idx = rewrite(idx);
        val = rewrite(val);
        world.DLOG("rewritten insert");
        world.DLOG("  mem: {} : {}", mem, mem->type());
        world.DLOG("  mat: {} : {}", mat, mat->type());
        world.DLOG("  idx: {} : {}", idx, idx->type());
        world.DLOG("  val: {} : {}", val, val->type());
        auto ptr_mat     = mat;
        auto element_ptr = op_lea_tuple(ptr_mat, idx);
        auto mem2        = mem::op_store(mem, element_ptr, val);
        // return mem2, ptr_mat);
        return world.tuple({mem2, ptr_mat});
    } else if (auto const_ax = match<matrix::constMat>(def)) {
        auto [mem, val]      = const_ax->args<2>();
        mem                  = rewrite(mem);
        val                  = rewrite(val);
        auto [n_def, S, T]   = const_ax->callee()->as<App>()->args<3>();
        S                    = rewrite(S);
        T                    = rewrite(T);
        auto arr_ty          = arrTyOfMatrixTy(S, T);
        auto [mem2, ptr_mat] = mem::op_alloc(arr_ty, mem)->projs<2>();

        // store initial value
        auto n       = n_def->as<Lit>()->get<u64>();
        auto initial = op_pack_tuple(n, S, val);

        // TODO: test if this is a valid initialization
        auto mem3 = mem::op_store(mem2, ptr_mat, initial);

        return world.tuple({mem3, ptr_mat});
    }

    // ignore unapplied axioms to avoid spurious type replacements
    if (auto ax = def->isa<Axiom>()) { return def; }

    return Rewriter::rewrite_structural(def); // continue recursive rewriting with everything else
}

} // namespace thorin::matrix
