#include "dialects/matrix/passes/lower_matrix.h"

#include <iostream>

#include <thorin/lam.h>
#include <thorin/tables.h>

#include "dialects/affine/affine.h"
#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/matrix/matrix.h"
#include "dialects/mem/mem.h"

namespace thorin::matrix {

const Def* LowerMatrix::rewrite(const Def* def) {
    if (auto i = rewritten.find(def); i != rewritten.end()) return i->second;
    rewritten[def] = rewrite_(def);
    return rewritten[def];
}

// TODO: documentation (arguments, functionality, for control flow, for arguments)
// TODO: generalize to general start, step, accumulators
Lam* multifor(World& world, Array<const Def*> bounds, const Def* inner_body) {
    auto count = bounds.size();
    Array<const Def*> iterators(count);
    auto I32         = world.type_int_width(32);
    Defs empty_tuple = {};
    auto empty_type  = world.tuple(empty_tuple)->type(); // TODO: check
    auto res_ty      = world.cn({mem::type_mem(world), empty_type});
    auto iter_ty     = world.cn({mem::type_mem(world), I32, empty_type, res_ty});

    auto outer_ty = world.cn({mem::type_mem(world), empty_type, res_ty});

    auto outer_container   = world.nom_lam(outer_ty, world.dbg("outer"));
    auto [mem, acc, yield] = outer_container->vars<3>();

    auto zero_lit = world.lit_int_width(32, 0, world.dbg("zero"));
    auto one_lit  = world.lit_int_width(32, 1, world.dbg("one"));

    Lam* container = outer_container;

    Lam* for_body;
    for (size_t i = 0; i < count; ++i) {
        for_body  = world.nom_lam(iter_ty, world.dbg("container_" + std::to_string(i)));
        auto call = affine::op_for(world, mem, zero_lit, bounds[i], one_lit, empty_tuple, for_body, yield);

        container->set_body(call);
        container->set_filter(true);
        container    = for_body;
        mem          = container->var(0, world.dbg("mem"));
        auto idx     = container->var(1, world.dbg("idx"));
        acc          = container->var(2, world.dbg("acc"));
        yield        = container->var(3, world.dbg("yield"));
        iterators[i] = idx;
    }
    container->app(true, inner_body, {mem::mem_var(container), world.tuple(iterators), acc, yield});

    return outer_container;
}

// TODO: compare with other impala version (why is one easier than the other?)
const Def* LowerMatrix::rewrite_(const Def* def) {
    // std::cout << "rewriting " << def << std::endl;

    auto& world = def->world();

    if (auto mapReduce_ax = match<matrix::mapReduce>(def); mapReduce_ax) {
        auto mapReduce_pi = mapReduce_ax->callee_type();

        auto args                         = mapReduce_ax->arg();
        auto [mem, zero, add, mul, input] = mapReduce_ax->args<5>(
            {world.dbg("mem"), world.dbg("zero"), world.dbg("add"), world.dbg("mul"), world.dbg("input")});

        world.DLOG("rewriting mapReduce axiom: {}\n", mapReduce_ax);
        world.DLOG("  zero: {}\n", zero);
        world.DLOG("  add: {}\n", add);
        world.DLOG("  mul: {}\n", mul);
        world.DLOG("  input: {}\n", input);

        auto inner_callee = mapReduce_ax->callee()->as<App>();

        auto [n, S, T, m, NI, TI, SI] =
            inner_callee->args<7>({world.dbg("n"), world.dbg("S"), world.dbg("T"), world.dbg("m"), world.dbg("NI"),
                                   world.dbg("TI"), world.dbg("SI")});

        auto n_lit = as_lit(n);
        auto m_lit = as_lit(m);

        auto zero_lit    = world.lit_int_width(32, 0, world.dbg("zero"));
        auto one_lit     = world.lit_int_width(32, 1, world.dbg("one"));
        Defs empty_tuple = {};
        auto empty_type  = world.tuple(empty_tuple)->type(); // TODO: check

        auto I32 = world.type_int_width(32);

        // idx number (>n), max_size
        std::vector<std::pair<u64, const Def*>> inner_idxs;
        // TODO: collect other indices

        Array<Array<u64>> inner_access(m_lit);
        for (auto i = 0; i < m_lit; i++) {
            auto [access, imat] = input->proj(i)->projs<2>();
            auto access_size    = as_lit(world.extract(NI, i));
            Array<u64> indices(access_size);
            for (auto j = 0; j < access_size; j++) {
                indices[j] = as_lit(world.extract(access, j));
                if (indices[j] >= n_lit) {
                    auto max_size = world.extract(world.extract(SI, i), j);
                    inner_idxs.push_back({indices[j], max_size});
                }
            }
        }
        // TODO: check indices
        // TODO: check inner_idxs

        Array<const Def*> out_bounds(n_lit, [&](u64 i) {
            auto dim_nat = world.extract(S, i);
            auto dim_int = core::op_bitcast(I32, dim_nat);
            return dim_int;
        });

        Array<const Def*> inner_bounds(inner_idxs.size(), [&](u64 i) {
            auto dim_nat = inner_idxs[i].second;
            auto dim_int = core::op_bitcast(I32, dim_nat);
            return dim_int;
        });

        auto res_ty              = world.cn({mem::type_mem(world), empty_type});
        auto inner_idx_count_nat = world.lit_nat(inner_idxs.size());

        auto middle_type    = world.cn({mem::type_mem(world), world.arr(n, I32), empty_type, res_ty});
        auto innermost_type = world.cn({mem::type_mem(world), world.arr(inner_idx_count_nat, I32), empty_type, res_ty});

        auto innermost_body = world.nom_lam(innermost_type, world.dbg("innermost"));
        auto middle_body    = world.nom_lam(middle_type, world.dbg("middle"));

        // TODO: check types

        auto outer_for = multifor(world, out_bounds, middle_body);
        auto inner_for = multifor(world, inner_bounds, innermost_body);

        auto [mid_mem, out_idx, mid_acc, mid_yield] = middle_body->vars<4>();
        auto [inn_mem, inn_idx, inn_acc, inn_yield] = innermost_body->vars<4>();

        // out:
        // init matrix, call middle, return matrix

        Lam* outer_cont = world.nom_lam(res_ty, world.dbg("outer_cont"));

        // replaces axiom call function
        // TODO: cn instead of pi
        Lam* outer_container = world.nom_lam(world.pi(args->type(), def->type()), world.dbg("outer_container"));

        auto outer_mem             = mem::mem_var(outer_container);
        auto [outer_mem2, out_mat] = world.app(world.ax<matrix::init>(), {n, S, outer_mem, T})->projs<2>();

        // TODO: call outer_for(mem, [], out_cont)
        // out_cont: return matrix

        outer_container->app(true, outer_for, {outer_mem2, world.tuple(empty_tuple), outer_cont});
        // TODO: fill outer_cont

        // middle:
        // init sum, call inner loop, write sum to matrix
        auto mid_cont                     = world.nom_lam(res_ty, world.dbg("mid_cont"));
        auto [mid_cont_mem, mid_cont_acc] = mid_cont->vars<2>();

        DefArray out_idxs = out_idx->projs(n_lit);
        DefArray cast_out_idxs(n_lit);
        for (int i = 0; i < n_lit; i++) {
            auto dim_nat     = world.extract(S, i);
            cast_out_idxs[i] = core::op_bitcast(world.type_int(dim_nat), out_idxs[i]);
        }

        auto [mmem2, sum_ptr] = mem::op_alloc(zero->type(), mid_mem, world.dbg("sum"))->projs<2>();
        auto mmem3            = mem::op_store(mmem2, sum_ptr, zero, world.dbg("sum_0"));

        // set middle_body(mem, idxs, yield) to call call inner_for
        // call inner_for (mem, acc, mid_cont)

        middle_body->app(true, inner_for, {mmem3, mid_acc, mid_cont});

        auto [mid_cont_mem2, out_mat_tmp2] = world
                                                 .app(world.app(world.ax<matrix::insert>(), {n, S, T}),
                                                      {mid_cont_mem, out_mat, world.tuple(cast_out_idxs)})
                                                 ->projs<2>();

        mid_cont->app(true, mid_yield, {mid_cont_mem2, mid_cont_acc});

        // TODO: set inner_body to compute

        // TODO: create out_matrix in outer lam

        return def;

        // auto outer_iteration_call = multifor(world, bounds, inner_body);

        // auto iterTy = world.pi({mem::type_mem(), I32, empty_type}, )
        auto iter_ty = world.cn({mem::type_mem(world), I32, empty_type, res_ty});
        auto iter_pi = iter_ty->as<Pi>();

        Lam* container = world.nom_lam(iter_pi, world.dbg("inner_container"));
        // end continuation returning the resulting matrix
        Lam* outer_cont = world.nom_lam(world.pi(res_ty, world.tuple({})), world.dbg("outer_cont"));

        Lam* outer_container = world.nom_lam(world.pi(args->type(), def->type()), world.dbg("outer_container"));

        auto outer_mem             = mem::mem_var(outer_container);
        auto [outer_mem2, out_mat] = world.app(world.ax<matrix::init>(), {n, S, outer_mem, T})->projs<2>();

        // written in inner_cont
        outer_cont->app(true, outer_container->ret_var(), {mem::mem_var(outer_cont), out_mat});

        Lam* inner = container;

        // from inner loop to outer loop due to building restriction
        // output loops

        // TODO: rework when immutable arrays become a thing
        // TODO: generalize: iterate over index-array with sizes
        // transport out matrix
        for (int i = n_lit - 1; i >= 0; i--) {
            auto dim_nat = world.extract(S, i);
            auto dim_int = core::op_bitcast(I32, dim_nat);
            // acc = init
            // for i = start to end step by step
            //   acc = body acc
            // exit acc
            // TODO: check if exit/break is set up correctly
            auto fori   = affine::op_for(world, mem::mem_var(inner),
                                         // start, end, step
                                         zero_lit, dim_int, one_lit,
                                         // init, body, exit
                                         empty_tuple, inner, outer_cont);
            out_idxs[i] = inner->var(1);
            // TODO: check iterators
            if (i == 0) {
                inner = world.nom_lam(world.pi({mem::type_mem(world)}, {}), world.dbg("iter_" + std::to_string(i)));
            } else {
                inner = world.nom_lam(iter_pi, world.dbg("iter_" + std::to_string(i)));
            }
            inner->set_body(fori);
            // out_idxs[i] = world.lit_nat(i);
        }

        outer_container->app(true, inner, {mem::mem_var(outer_container)});

        // TODO: extract into own function to access in normalizer
        // or use slot
        auto [imem2, sum_ptr] = mem::op_alloc(zero->type(), mem::mem_var(container), world.dbg("sum"))->projs<2>();
        auto imem3            = mem::op_store(imem2, sum_ptr, zero, world.dbg("sum_0"));

        Lam* inner_cont = world.nom_lam(world.pi(res_ty, world.tuple({})), world.dbg("inner_cont"));
        // TODO: write sum to matrix in inner_cont

        DefArray cast_out_idxs(n_lit);
        for (int i = 0; i < n_lit; i++) {
            auto dim_nat     = world.extract(S, i);
            cast_out_idxs[i] = core::op_bitcast(world.type_int(dim_nat), out_idxs[i]);
        }

        auto [outer_mem2, out_mat_tmp2] = world
                                              .app(world.app(world.ax<matrix::insert>(), {n, S, T}),
                                                   {mem::mem_var(inner_cont), out_mat, world.tuple(cast_out_idxs)})
                                              ->projs<2>();

        // TODO: set container body to call inner for loop (with imem3)

        auto ret_def_call = direct::op_cps2ds(outer_container);
        // TODO: check
        auto ret_def = world.app(ret_def_call, args);

        return def;

        // auto& w = world();
        // w.DLOG("rewriting for axiom: {} within {}", for_ax, curr_nom());

        // auto for_pi  = for_ax->callee_type();
        // auto for_lam = w.nom_lam(for_pi, w.dbg("for"));

        // auto org_body  = for_ax->arg(for_ax->num_args() - 2);
        // auto body_type = org_body->type()->as<Pi>();
        // auto yield_pi  = body_type->doms().back()->as<Pi>();
        // auto yield_lam = w.nom_lam(yield_pi, w.dbg("yield"));

        // { // construct yield
        //     auto [mem, iter, end, step, acc, body, brk] =
        //         for_lam->vars<7>({w.dbg("mem"), w.dbg("begin"), w.dbg("end"), w.dbg("step"), w.dbg("acc"),
        //                           w.dbg("body"), w.dbg("break")});
        //     auto [yield_mem, yield_acc] = yield_lam->vars<2>();

        //     auto add = w.op(Wrap::add, w.lit_nat_0(), iter, step);
        //     yield_lam->app(false, for_lam, {yield_mem, add, end, step, yield_acc, body, brk});
        // }
        // { // construct for
        //     auto [mem, iter, end, step, acc, body, brk] = for_lam->vars<7>();

        //     // continue
        //     auto if_then_cn = w.cn(mem->type());
        //     auto if_then    = w.nom_lam(if_then_cn, nullptr);
        //     if_then->app(false, body, {if_then->var(0, w.dbg("mem")), iter, acc, yield_lam});

        //     // break
        //     auto if_else_cn = w.cn(mem->type());
        //     auto if_else    = w.nom_lam(if_else_cn, nullptr);
        //     if_else->app(false, brk, {if_else->var(0, w.dbg("mem")), acc});

        //     auto cmp = w.op(ICmp::ul, iter, end);
        //     for_lam->branch(false, cmp, if_then, if_else, mem);
        // }

        // return rewritten_[def] = w.app(for_lam, for_ax->arg(), for_ax->dbg());
    }

    return def;
}

PassTag* LowerMatrix::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::matrix
