#include "dialects/matrix/passes/lower_matrix_mediumlevel.h"

#include <iostream>

#include <thorin/lam.h>

#include "thorin/def.h"

#include "dialects/affine/affine.h"
#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/matrix/matrix.h"
#include "dialects/mem/mem.h"

namespace thorin::matrix {

const Def* LowerMatrixMediumLevel::rewrite(const Def* def) {
    if (auto i = rewritten.find(def); i != rewritten.end()) return i->second;
    auto new_def   = rewrite_(def);
    rewritten[def] = new_def;
    return rewritten[def];
}

std::pair<Lam*, const Def*>
counting_for(const Def* bound, DefArray acc, const Def* exit, const char* name = "for_body") {
    auto& world = bound->world();
    auto acc_ty = world.tuple(acc)->type();
    auto body   = world
                    .nom_lam(world.cn({
                        world.type_int(32), // iterator
                        acc_ty,             // acc = memory+extra
                        world.cn(acc_ty)    // exit = return
                    }))
                    ->set(name);
    auto for_loop = affine::op_for(world, world.lit_int(32, 0), bound, world.lit_int(32, 1), acc, body, exit);
    return {body, for_loop};
}

// TODO: compare with other impala version (why is one easier than the other?)
// TODO: replace sum_ptr by using sum as accumulator
// TODO: extract inner loop into function (for read normalizer)
const Def* LowerMatrixMediumLevel::rewrite_(const Def* def) {
    auto& world = def->world();

    if (auto mapReduce_ax = match<matrix::mapReduce>(def); mapReduce_ax) {
        // meta arguments:
        // * n = out-count, (nat)
        // * S = out-dim, (n*nat)
        // * T = out-type (*)
        // * m = in-count (nat)
        // * NI = in-dim-count (m*nat)
        // * TI = types (m**)
        // * SI = dimensions (m*NI#i)
        // arguments:
        // * mem
        // * zero = accumulator init (T)
        // * combination function (mem, acc, inputs) -> (mem, acc)
        // * input matrixes
        auto [mem, zero, comb, inputs] = mapReduce_ax->args<4>();
        auto [n, S, T, m, NI, TI, SI]  = mapReduce_ax->callee()->as<App>()->args<7>();
        world.DLOG("mapReduce_ax {} : {}", mapReduce_ax, mapReduce_ax->type());
        world.DLOG("meta variables:");
        world.DLOG("  n = {}", n);
        world.DLOG("  S = {}", S);
        world.DLOG("  T = {}", T);
        world.DLOG("  m = {}", m);
        world.DLOG("  NI = {} : {}", NI, NI->type());
        world.DLOG("  TI = {} : {}", TI, TI->type());
        world.DLOG("  SI = {} : {}", SI, SI->type());
        world.DLOG("arguments:");
        world.DLOG("  mem = {}", mem);
        world.DLOG("  zero = {}", zero);
        world.DLOG("  comb = {} : {}", comb, comb->type());
        world.DLOG("  inputs = {} : {}", inputs, inputs->type());

        // Goal: generate call to function that performs:
        // ```
        // matrix = new matrix (n, S, T)
        // for out_idx { // n for loops
        //     acc = zero
        //     for in_idx { // remaining loops
        //         inps = read from matrices // m-tuple
        //         acc = comb(mem, acc, inps)
        //     }
        //     write acc to output matrix
        // }
        // return matrix
        // ```

        std::map<u64, const Def*> dims;         // idx ↦ nat (size bound = dimension)
        std::map<u64, const Def*> raw_iterator; // idx ↦ I32
        std::map<u64, const Def*> iterator;     // idx ↦ %Idx (S/NI#i)
        std::vector<u64> out_indices;           // output indices 0..n-1
        std::vector<u64> in_indices;            // input indices ≥ n

        std::vector<const Def*> output_dims;             // i<n ↦ nat (dimension S#i)
        std::vector<std::vector<const Def*>> input_dims; // i<m ↦ j<NI#i ↦ nat (dimension SI#i#j)
        std::vector<u64> n_input;                        // i<m ↦ nat (number of dimensions of SI#i)

        auto n_lit = n->isa<Lit>();
        auto m_lit = m->isa<Lit>();
        if (!n_lit || !m_lit) {
            world.DLOG("n or m is not a literal");
            return def;
        }

        auto n_nat = n_lit->get<u64>(); // number of output dimensions (in S)
        auto m_nat = m_lit->get<u64>(); // number of input matrices

        // collect out dimensions
        world.DLOG("out dims (n) = {}", n_nat);
        for (u64 i = 0; i < n_nat; ++i) {
            auto dim = S->proj(n_nat, i);
            world.DLOG("dim {} = {}", i, dim);
            dims[i] = dim;
            output_dims.push_back(dim);
        }

        // collect other (input) dimensions
        world.DLOG("matrix count (m) = {}", m_nat);

        for (u64 i = 0; i < m_nat; ++i) {
            auto ni     = NI->proj(m_nat, i);
            auto ni_lit = ni->isa<Lit>();
            if (!ni_lit) {
                world.DLOG("matrix {} has non-constant dimension count", i);
                return def;
            }
            auto ni_nat = ni_lit->get<u64>();
            world.DLOG("  dims({i}) = {}", i, ni_nat);
            auto SI_i = SI->proj(m_nat, i);
            std::vector<const Def*> input_dims_i;
            for (u64 j = 0; j < ni_nat; ++j) {
                auto dim = SI_i->proj(ni_nat, j);
                world.DLOG("    dim {} {} = {}", i, j, dim);
                // dims[i * n_nat + j] = dim;
                input_dims_i.push_back(dim);
            }
            input_dims.push_back(input_dims_i);
            n_input.push_back(ni_nat);
        }

        // extracts bounds for each index (in, out)
        for (u64 i = 0; i < m_nat; ++i) {
            world.DLOG("investigate {} / {}", i, m_nat);
            auto [indices, mat] = inputs->proj(m_nat, i)->projs<2>();
            world.DLOG("  indices {} = {}", i, indices);
            world.DLOG("  matrix {} = {}", i, mat);
            for (u64 j = 0; j < n_input[i]; ++j) {
                // world.DLOG("    dimension {} / {}", j, n_input[i]);
                auto idx     = indices->proj(n_input[i], j);
                auto idx_lit = idx->isa<Lit>();
                if (!idx_lit) {
                    world.DLOG("    index {} {} is not a literal", i, j);
                    return def;
                }
                auto idx_nat = idx_lit->get<u64>();
                auto dim     = input_dims[i][j];
                world.DLOG("      index {} = {}", j, idx);
                world.DLOG("        dim {} = {}", idx, dim);
                if (!dims.contains(idx_nat)) {
                    dims[idx_nat] = dim;
                    world.DLOG("        {} ↦ {}", idx_nat, dim);
                } else {
                    // assert(dims[idx_nat] == dim);
                    auto prev_dim = dims[idx_nat];
                    world.DLOG("        prev dim {} = {}", idx_nat, prev_dim);
                    // override with more precise information
                    if (auto dim_lit = dim->isa<Lit>()) {
                        if (auto prev_dim_lit = prev_dim->isa<Lit>())
                            assert(dim_lit->get<u64>() == prev_dim_lit->get<u64>() && "dimensions must be equal");
                        else
                            dims[idx_nat] = dim;
                    }
                }
            }
        }

        for (auto [idx, dim] : dims) {
            world.DLOG("dim {} = {}", idx, dim);
            if (idx < n_nat)
                out_indices.push_back(idx);
            else
                in_indices.push_back(idx);
        }

        // create function `%mem.M -> [%mem.M, %matrix.Mat (n,S,T)]` to replace axiom call

        auto mem_type = mem::type_mem(world);
        auto fun_ty   = world.cn({mem_type, world.cn(mapReduce_ax->type())});
        world.DLOG("fun_ty = {}", fun_ty);
        auto fun = world.nom_lam(fun_ty)->set("mapRed");

        // assert(0);
        auto ds_fun = direct::op_cps2ds_dep(fun);
        world.DLOG("ds_fun {} : {}", ds_fun, ds_fun->type());
        auto call = world.app(ds_fun, mem);
        world.DLOG("call {} : {}", call, call->type());

        // flowchart:
        // ```
        // -> init
        // -> forOut1 with yieldOut1
        //    => exitOut1 = return_cont
        // -> forOut2 with yieldOut2
        //    => exitOut2 = yieldOut1
        // -> ...
        // -> accumulator init
        // -> forIn1 with yieldIn1
        //    => exitIn1 = writeCont
        // -> forIn2 with yieldIn2
        //    => exitIn2 = yieldIn1
        // -> ...
        // -> read matrices
        // -> fun
        //    => exitFun = yieldInM
        //
        // (return path)
        // -> ...
        // -> write
        // -> yieldOutN
        // -> ...
        // ```

        // First create the output matrix.
        auto current_mem      = mem;
        auto [mem2, init_mat] = world.app(world.ax<matrix::init>(), {n, S, T, current_mem})->projs<2>();
        current_mem           = mem2;

        // The function on where to continue -- return after all output loops.
        auto cont        = fun->var(1);
        auto current_nom = fun;

        // Each of the outer loops contains the memory and matrix as accumulator (in an inner monad).
        DefArray acc = {current_mem, init_mat};

        for (auto idx : out_indices) {
            char for_name[32];
            sprintf(for_name, "forOut_%lu", idx);

            auto dim_nat_def = dims[idx];
            auto dim         = core::op_bitcast(world.type_int(32), dim_nat_def);

            auto [body, for_call]       = counting_for(dim, acc, cont, for_name);
            auto [iter, new_acc, yield] = body->vars<3>();
            cont                        = yield;
            raw_iterator[idx]           = iter;
            iterator[idx]               = core::op_bitcast(world.type_idx(dim_nat_def), iter);
            auto [new_mem, new_mat]     = new_acc->projs<2>();
            acc                         = {new_mem, new_mat};
            current_nom->set_body(for_call);
            current_nom->set_filter(dim_nat_def);
            current_nom = body;
        }

        // Now the inner loops for the inputs:
        // Each of the inner loops contains the element accumulator and memory as accumulator (in an inner monad).
        world.DLOG("acc at inner: {;}", acc);

        // First create the accumulator.
        auto element_acc = zero;
        element_acc->set("acc");
        current_mem    = acc[0];
        auto wb_matrix = acc[1];
        // world.DLOG("wb_matrix {} ", wb_matrix);
        assert(wb_matrix);
        world.DLOG("wb_matrix {} : {}", wb_matrix, wb_matrix->type());
        // world.DLOG("acc[1] at inner: {} : {}", acc[1], acc[1]->type());

        // Write back element to matrix. Set this as return after all inner loops.
        auto write_back = world.nom_lam(world.cn({mem::type_mem(world), T}))->set("matrixWriteBack");
        // TODO: why is acc no longer valid from here on?
        world.DLOG("write_back {} : {}", write_back, write_back->type());
        // world.DLOG("acc[1] at inner: {} : {}", acc[1], acc[1]->type());
        auto [wb_mem, element_final] = write_back->vars<2>();
        // world.DLOG("acc[1] at inner: {} : {}", acc[1], acc[1]->type());
        // world.DLOG("acc[1] at inner: {} : {}", acc[1], acc[1]->type());

        DefArray output_iterators((size_t)n_nat, [&](u64 i) {
            auto idx = out_indices[i];
            assert(idx == i && "output indices must be consecutive 0..n-1");
            // auto iter_int_def = raw_iterator[idx];
            // auto dim          = dims[idx];
            // world.DLOG("dim of {} = {}", i, dim);
            // return iter_int_def;
            // auto iter_idx_def = core::op_bitcast(world.type_idx(dim), iter_int_def);
            auto iter_idx_def = iterator[idx];
            return iter_idx_def;
        });
        auto output_it_tuple = world.tuple(output_iterators);
        world.DLOG("output tuple: {} : {}", output_it_tuple, output_it_tuple->type());

        auto [wb_mem2, written_matrix] = world
                                             .app(world.app(world.ax<matrix::insert>(), {n, S, T}),
                                                  {wb_mem, wb_matrix, output_it_tuple, element_final})
                                             ->projs<2>();

        write_back->app(true, cont, {wb_mem2, written_matrix});

        // From here on the continuations take the element and memory.
        acc  = {current_mem, element_acc};
        cont = write_back;

        for (auto idx : in_indices) {
            char for_name[32];
            sprintf(for_name, "forIn_%lu", idx);

            auto dim_nat_def = dims[idx];
            auto dim         = core::op_bitcast(world.type_int(32), dim_nat_def);

            auto [body, for_call]       = counting_for(dim, acc, cont, for_name);
            auto [iter, new_acc, yield] = body->vars<3>();
            cont                        = yield;
            raw_iterator[idx]           = iter;
            iterator[idx]               = core::op_bitcast(world.type_idx(dim_nat_def), iter);
            auto [new_mem, new_element] = new_acc->projs<2>();
            acc                         = {new_mem, new_element};
            current_nom->set_body(for_call);
            current_nom->set_filter(dim_nat_def);
            current_nom = body;
        }

        // For testing: id in innermost loop instead of read, fun:
        // current_nom->app(true, cont, acc);

        current_mem = acc[0];
        element_acc = acc[1];

        // Read element from input matrix.
        DefArray input_elements((size_t)m_nat);
        for (u64 i = 0; i < m_nat; i++) {
            // TODO: case m_nat == 1
            auto input_i                       = inputs->proj(m_nat, i);
            auto [input_idx_tup, input_matrix] = input_i->projs<2>();

            world.DLOG("input matrix {} is {} : {}", i, input_matrix, input_matrix->type());

            auto indices = input_idx_tup->projs(n_input[i]);
            DefArray input_iterators(n_input[i], [&](u64 j) {
                auto idx     = indices[j];
                auto idx_lit = idx->as<Lit>()->get<u64>();
                world.DLOG("  idx {} {} = {}", i, j, idx_lit);
                return iterator[idx_lit];
            });
            auto input_it_tuple = world.tuple(input_iterators);

            auto read_entry = op_read(current_mem, input_matrix, input_it_tuple);
            world.DLOG("read_entry {} : {}", read_entry, read_entry->type());
            auto [new_mem, element_i] = read_entry->projs<2>();
            current_mem               = new_mem;
            input_elements[i]         = element_i;
        }

        world.DLOG("  read elements {,}", input_elements);
        world.DLOG("  fun {} : {}", fun, fun->type());

        // current_nom->app(true, cont, {current_mem, element_acc});
        // TODO: make non-scalar or completely scalar?
        current_nom->app(true, comb, {world.tuple({current_mem, element_acc, world.tuple(input_elements)}), cont});
        // current_nom->app(true, comb, {current_mem, element_acc, world.tuple(input_elements), cont});

        return call;

        // create out iterations
    }

    return def;
}

} // namespace thorin::matrix
