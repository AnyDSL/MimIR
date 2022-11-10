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
    // if (auto app = def->isa<App>()) {
    //     if (is_weird(app)) return app; // stop recursing here
    //     if (is_crazy(app)) {
    //         auto callee = rewrite(app->callee()); // recursively rewrite callee
    //         auto arg    = app->arg();             // don't recurse here for whatever reason
    //         return my_other_crazy_app(callee, arg);
    //     }
    //     // note the fallthrough here
    // }
    auto& world = def->world();

    assert(!match<matrix::mapReduce>(def) && "mapReduce should have been lowered to for loops by now");
    assert(!match<matrix::shape>(def) && "high level operations should have been lowered to for loops by now");
    assert(!match<matrix::prod>(def) && "high level operations should have been lowered to for loops by now");
    assert(!match<matrix::transpose>(def) && "high level operations should have been lowered to for loops by now");
    assert(!match<matrix::sum>(def) && "high level operations should have been lowered to for loops by now");
    // assert(!match<matrix::Mat>(def) && "expected failure");

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

        // DefArray new_ops{def->ops(), [&](const Def* op) { return rewrite_def(op); }};
        // auto aug_mat_ax = def->rebuild(world, def->type(), new_ops, def->dbg());

        // auto arr_ty = arrTyOfMatrixTy(aug_mat_ax);
        auto [_, S, T] = mat_ax->args<3>();
        S              = rewrite(S);
        T              = rewrite(T);
        auto arr_ty    = arrTyOfMatrixTy(S, T);

        auto addr_space = world.lit_nat_0();
        auto ptr_ty     = world.app(world.ax<mem::Ptr>(), {arr_ty, addr_space});

        return ptr_ty;
    } else if (auto init_ax = match<matrix::init>(def)) {
        auto [_, S, T, mem]  = init_ax->args<4>();
        S                    = rewrite(S);
        T                    = rewrite(T);
        mem                  = rewrite(mem);
        auto arr_ty          = arrTyOfMatrixTy(S, T);
        auto [mem2, ptr_mat] = mem::op_alloc(arr_ty, mem)->projs<2>();
        return world.tuple({mem2, ptr_mat});
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
        auto ptr_mat     = rewrite(mat);
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
        auto ptr_mat     = mat; // rewrite(mat);
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

    return Rewriter::rewrite_structural(def); // continue recursive rewriting with everything else
}

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

// void LowerMatrixLowLevel::enter() { rewrite_lam(curr_nom()); }
// void LowerMatrixLowLevel::rewrite_lam(Lam* lam) { lam->set_body(rewrite_def(lam->body())); }

// const Def* LowerMatrixLowLevel::rewrite_def(const Def* def) {
//     // auto& world = def->world();
//     // world.DLOG("rewrite {} : {}", def, def->type());
//     if (auto i = rewritten.find(def); i != rewritten.end()) return i->second;
//     auto new_def = rewrite_def_(def);
//     // if (def->type() != new_def->type()) new_def = core::op_bitcast(def->type(), new_def);
//     rewritten[def] = new_def;
//     return rewritten[def];
// }

// enum NOpKind { add, mul };

// const Def* op_nop(const Def* a, const Def* b, NOpKind kind) {
//     auto& world = a->world();
//     return world.app(world.ax(kind == add ? core::nop::add : core::nop::mul), {a, b});

//     // auto I32   = world.type_int(32);
//     // auto a_i32 = core::op_bitcast(I32, a);
//     // auto b_i32 = core::op_bitcast(I32, b);
//     // auto c_i32 = world.app(world.app(world.ax(kind == add ? core::nop::add : core::nop::mul),
//     //                                  {world.lit_nat_0(), world.lit_nat(bitwidth2size(32))}),
//     //                        {a_i32, b_i32});
//     // auto c     = core::op_bitcast(world.type_nat(), c_i32);
//     // return c;
// }

// // const Def* computeSize(const Def* S) {
// //     auto& world = S->world();
// //     auto n      = S->num_projs();
// //     world.DLOG("compute Size of {} ({} dims)", S, n);
// //     const Def* size = world.lit_nat_1();
// //     for (size_t i = 0; i < n; i++) {
// //         auto dim = S->proj(i);
// //         // world.DLOG("dim {}: {}", i, dim);
// //         // size = world.app(world.ax(core::nop::mul), {size, dim});
// //         size = op_nop(size, dim, mul);
// //     }

// //     // assert(0);
// //     // size = world.lit_nat(42);
// //     return size;
// // }

// // const Def* sizeOfMatrix(const Def* Mat) {
// //     auto mat_ax = match<matrix::Mat>(Mat);
// //     assert(mat_ax && "type must be a matrix");
// //     auto [n_def, S, T] = mat_ax->args<3>();
// //     return computeSize(S);
// // }

// // void LowerMatrixLowLevel::enter() {
// //     if (!curr_nom()->is_external()) return;
// //     auto lam = curr_nom()->isa_nom<Lam>();
// //     if (!lam) return;
// //     auto rewritten_pi  = rewrite(lam->type())->as<Pi>();
// //     auto rewritten_lam = world().nom_lam(rewritten_pi);
// //     rewritten_lam->set_body(rewrite(lam->body()));
// // }

// const Def* LowerMatrixLowLevel::rewrite_def_(const Def* def) {
//     if (!def) return def;
//     // std::cout << def->node_name() << std::endl;
//     // std::cout << def << std::endl;
//     // try {
//     //     auto& i_world = (def->world());
//     // } catch (std::exception& e) { return def; }
//     // if (def->world().empty()) return def;
//     // if (def->isa<Type>()) return def;
//     // if (def->isa<Univ>()) return def;
//     // if (def->type()->isa<Univ>()) return def;
//     auto& world = def->world();

//     assert(!match<matrix::mapReduce>(def) && "mapReduce should have been lowered to for loops by now");
//     assert(!match<matrix::shape>(def) && "high level operations should have been lowered to for loops by now");
//     assert(!match<matrix::prod>(def) && "high level operations should have been lowered to for loops by now");
//     assert(!match<matrix::transpose>(def) && "high level operations should have been lowered to for loops by now");
//     assert(!match<matrix::sum>(def) && "high level operations should have been lowered to for loops by now");

//     if (auto lam = def->isa_nom<Lam>()) {
//         world.DLOG("lower lam {} : {}", lam, lam->type());
//         auto ty     = lam->type();
//         auto new_ty = rewrite_def(ty);

//         world.DLOG("new ty {}", new_ty);
//         auto new_lam          = world.nom_lam(new_ty->as<Pi>(), lam->dbg());
//         rewritten[lam->var()] = new_lam->var();
//         rewritten[lam]        = new_lam;
//         world.DLOG("assoc {} -> {}", lam, new_lam);
//         world.DLOG("assoc {} -> {}", lam->var(), new_lam->var());
//         new_lam->set_body(rewrite_def(lam->body()));
//         // new_lam->set_filter(lam->filter());
//         new_lam->set_filter(rewrite_def(lam->filter()));
//         // new_lam->set_filter(false);
//         return new_lam;
//         // lam->set_type(new_ty);
//         // assert(0);
//         // rewrite_lam(lam);
//     }

//     // world.DLOG("inspect {} : {}", def, def->type());

//     if (auto mat_ax = match<matrix::Mat>(def)) {
//         // auto [n_def, S, T] = mat_ax->args<3>();
//         world.DLOG("Lowering Mat {} to Ptr", mat_ax);
//         // auto n = (size_t)(n_def->as<Lit>()->get<u64>());

//         // const Def* size = world.app(world.ax<core::nop>(core::nop::mul), {S->proj(0), S->proj(1)});
//         // const Def* size2 = S->proj(0);
//         // world.DLOG("size2: {} : {}", size2, size2->type());
//         // auto size = computeSize(S);

//         // world.DLOG("size: {} : {}", size, size->type());

//         // auto mat_ty = world.app(world.ax<Mat>(), {world.lit_nat_1(), size, T});
//         // return mat_ty;

//         // TODO: why does replacement not take effect
//         // return world.type_nat();

//         // auto arr_ty = world.arr(size, T);

//         // DefArray new_ops{def->ops(), [&](const Def* op) { return rewrite_def(op); }};
//         // auto aug_mat_ax = def->rebuild(world, def->type(), new_ops, def->dbg());

//         // auto arr_ty = arrTyOfMatrixTy(aug_mat_ax);
//         auto [_, S, T] = mat_ax->args<3>();
//         S              = rewrite_def(S);
//         T              = rewrite_def(T);
//         auto arr_ty    = arrTyOfMatrixTy(S, T);

//         auto addr_space = world.lit_nat_0();
//         auto ptr_ty     = world.app(world.ax<mem::Ptr>(), {arr_ty, addr_space});

//         return ptr_ty;
//     } else if (auto init_ax = match<matrix::init>(def)) {
//         auto [_, S, T, mem]  = init_ax->args<4>();
//         S                    = rewrite_def(S);
//         T                    = rewrite_def(T);
//         mem                  = rewrite_def(mem);
//         auto arr_ty          = arrTyOfMatrixTy(S, T);
//         auto [mem2, ptr_mat] = mem::op_alloc(arr_ty, mem)->projs<2>();
//         return world.tuple({mem2, ptr_mat});
//     } else if (auto read_ax = match<matrix::read>(def)) {
//         auto [mem, mat, idx] = read_ax->args<3>();
//         mem                  = rewrite_def(mem);
//         mat                  = rewrite_def(mat);
//         idx                  = rewrite_def(idx);
//         // TODO: check if mat is already converted
//         auto ptr_mat     = rewrite_def(mat);
//         auto element_ptr = op_lea_tuple(ptr_mat, idx);
//         auto [mem2, val] = mem::op_load(mem, element_ptr)->projs<2>();
//         return world.tuple({mem2, val});
//     } else if (auto insert_ax = match<matrix::insert>(def)) {
//         auto [mem, mat, idx, val] = insert_ax->args<4>();
//         mem                       = rewrite_def(mem);
//         mat                       = rewrite_def(mat);
//         idx                       = rewrite_def(idx);
//         val                       = rewrite_def(val);
//         auto ptr_mat              = rewrite_def(mat);
//         auto element_ptr          = op_lea_tuple(ptr_mat, idx);
//         auto mem2                 = mem::op_store(mem, element_ptr, val);
//         // return mem2, ptr_mat);
//         return world.tuple({mem2, ptr_mat});
//     } else if (auto const_ax = match<matrix::constMat>(def)) {
//         auto [mem, val]      = const_ax->args<2>();
//         mem                  = rewrite_def(mem);
//         val                  = rewrite_def(val);
//         auto [n_def, S, T]   = const_ax->callee()->as<App>()->args<3>();
//         S                    = rewrite_def(S);
//         T                    = rewrite_def(T);
//         auto arr_ty          = arrTyOfMatrixTy(S, T);
//         auto [mem2, ptr_mat] = mem::op_alloc(arr_ty, mem)->projs<2>();

//         // store initial value
//         auto n       = n_def->as<Lit>()->get<u64>();
//         auto initial = op_pack_tuple(n, S, val);

//         // TODO: test if this is a valid initialization
//         auto mem3 = mem::op_store(mem2, ptr_mat, initial);

//         return world.tuple({mem3, ptr_mat});
//     }

//     if (auto app = def->isa<App>()) {
//         auto new_arg   = rewrite_def(app->arg());
//         auto new_calle = rewrite_def(app->callee());
//         return world.app(new_calle, new_arg);
//     }

//     // if (auto pack = def->isa<Pack>()) {
//     //     // Pack needs special care as the shape is not an operand
//     //     auto shape     = pack->shape();
//     //     auto body      = pack->body();
//     //     auto new_shape = rewrite_def(shape);
//     //     auto new_body  = rewrite_def(body);
//     //     return world.pack(new_shape, new_body, pack->dbg());
//     // }

//     // world.DLOG("unmodified {}", def);

//     if (auto var = def->isa<Var>()) { return var; }

//     // if (auto old_nom = def->isa_nom()) { return old_nom; }

//     // if (def->isa<Pi>()) { return def; }

//     // world.DLOG("unmodified name {}", def->node_name());
//     world.DLOG("info {}", def->node_name());
//     world.DLOG("unmodified {}", def);
//     world.DLOG("unmodified {} : {}", def, def->type());
//     world.DLOG("unmodified ops {, }", def->ops());
//     // world.DLOG("unmodified {} : {}", def, def->type());
//     DefArray new_ops{def->ops(), [&](const Def* op) { return rewrite_def(op); }};
//     if (def->isa<Tuple>()) return world.tuple(new_ops, def->dbg());

//     // return def->rebuild(world, def->type(), new_ops, def->dbg());
//     auto type = def->type();
//     const Def* new_type;
//     if (type != nullptr && !(type->isa<Type>())) {
//         // (def->isa_nom())
//         // !(type->isa<Pi>())) {
//         new_type = rewrite_def(type);
//     } else {
//         new_type = type;
//     }
//     return def->rebuild(world, new_type, new_ops, def->dbg());

//     // return def;
// }

// PassTag* LowerMatrixLowLevel::ID() {
//     static PassTag Key;
//     return &Key;
// }

} // namespace thorin::matrix
