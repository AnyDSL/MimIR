#include "mim/plug/matrix/pass/lower_matrix_lowlevel.h"

#include <cassert>

#include <mim/axm.h>
#include <mim/def.h>
#include <mim/lam.h>

#include "mim/plug/affine/affine.h"
#include "mim/plug/core/core.h"
#include "mim/plug/direct/direct.h"
#include "mim/plug/matrix/matrix.h"
#include "mim/plug/mem/mem.h"

namespace mim::plug::matrix {

namespace {

const Def* op_lea_tuple(const Def* arr, const Def* tuple) {
    auto& world = arr->world();
    world.DLOG("op_lea_tuple arr {} : {}", arr, arr->type());
    auto n       = tuple->num_projs();
    auto element = arr;
    for (size_t i = 0; i < n; ++i)
        element = mem::op_lea(element, tuple->proj(n, i));
    return element;
}

const Def* op_pack_tuple(u64 n, const Def* tuple, const Def* val) {
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

const Def* arr_ty_of_matrix_ty(const Def* S, const Def* T) {
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

const Def* LowerMatrixLowLevel::rewrite_app(const App* def) {
    assert(!Axm::isa<matrix::map_reduce>(def) && "map_reduce should have been lowered to for loops by now");
    assert(!Axm::isa<matrix::shape>(def) && "high level operations should have been lowered to for loops by now");
    assert(!Axm::isa<matrix::prod>(def) && "high level operations should have been lowered to for loops by now");
    assert(!Axm::isa<matrix::transpose>(def) && "high level operations should have been lowered to for loops by now");
    assert(!Axm::isa<matrix::sum>(def) && "high level operations should have been lowered to for loops by now");

    // TODO: generalize arg rewrite
    if (auto mat_ax = Axm::isa<matrix::Mat>(def)) {
        auto [_, S, T] = mat_ax->args<3>();
        S              = rewrite(S);
        T              = rewrite(T);
        auto arr_ty    = arr_ty_of_matrix_ty(S, T);

        auto addr_space = world().lit_nat_0();
        auto ptr_ty     = world().call<mem::Ptr>(Defs{arr_ty, addr_space});

        return ptr_ty;
    } else if (auto init_ax = Axm::isa<matrix::init>(def)) {
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
    } else if (auto read_ax = Axm::isa<matrix::read>(def)) {
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
    } else if (auto insert_ax = Axm::isa<matrix::insert>(def)) {
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
    } else if (auto const_ax = Axm::isa<matrix::constMat>(def)) {
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

    return Rewriter::rewrite_app(def); // continue recursive rewriting with everything else
}

} // namespace mim::plug::matrix
