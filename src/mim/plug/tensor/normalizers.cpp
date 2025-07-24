#include <mim/plug/affine/affine.h>
#include <mim/plug/core/core.h>
#include <mim/plug/direct/direct.h>
#include <mim/plug/tensor/autogen.h>

#include "mim/def.h"
#include "mim/world.h"

#include "mim/plug/tensor/tensor.h"

namespace mim::plug::tensor {

const Def* op_get(const Def* T, const Def* r, const Def* s, const Def* arr, const Def* index) {
    auto& w = arr->world();
    auto f  = w.annex<tensor::get>();
    f       = w.app(f, {T, r, s});
    f       = w.app(f, {arr, index});
    return f;
}

const Def* op_set(const Def* T, const Def* r, const Def* s, const Def* arr, const Def* index, const Def* x) {
    auto& w = arr->world();
    auto f  = w.app(w.annex<tensor::set>(), {T, r, s});
    f       = w.app(f, {arr, index, x});
    return f;
}

const Def* normalize_get(const Def*, const Def* c, const Def* arg) {
    auto& w = c->world();

    auto [arr, index] = arg->projs<2>();
    auto callee       = c->as<App>();
    auto [T, r, s]    = callee->args<3>();

    w.DLOG("normalize_get");
    w.DLOG("    arr = {} : {}", arr, arr->type());
    w.DLOG("    index = {} : {}", index, index->type());
    w.DLOG("    T = {} : {}", T, T->type());
    w.DLOG("    r = {} : {}", r, r->type());
    w.DLOG("    s = {} : {}", s, s->type());

    auto size = index->num_projs();
    w.DLOG("size = {}", size);
    if (size == 1) {
        w.DLOG("index of size 1, extract");
        return w.extract(arr, index);
    }

    auto r_lit = r->isa<Lit>();
    if (!r_lit) return nullptr;
    auto r_nat     = r_lit->get<u64>();
    auto new_r_nat = r_nat - 1;

    auto idx = index->proj(r_nat, 0);
    w.DLOG("idx = {} : {}", idx, idx->type());

    auto new_r     = w.lit_nat(new_r_nat);
    auto new_s_vec = DefVec(new_r_nat, [&](size_t i) { return s->proj(r_nat, i + 1); });
    auto new_s     = w.tuple(new_s_vec);

    auto new_index_vec = DefVec(new_r_nat, [&](size_t i) { return index->proj(r_nat, i + 1); });
    auto new_index     = w.tuple(new_index_vec);

    auto idx_n   = idx->type()->op(1);
    auto idx_lit = idx_n->isa<Lit>();
    if (!idx_lit) return nullptr;
    if (idx_lit->get<u64>() == 1) return op_get(T, new_r, new_s, arr, new_index);

    auto new_arr = w.extract(arr, idx);

    return op_get(T, new_r, new_s, new_arr, new_index);
}

const Def* normalize_set(const Def*, const Def* c, const Def* arg) {
    auto& w = c->world();

    auto [arr, index, x] = arg->projs<3>();
    w.DLOG("normalize_set");
    w.DLOG("    arr = {} : {}", arr, arr->type());
    w.DLOG("    index = {} : {}", index, index->type());
    w.DLOG("    x = {} : {}", x, x->type());

    auto size = index->num_projs();
    w.DLOG("    size = {}", size);
    if (size == 1) {
        w.DLOG("index of size 1, insert");
        return w.insert(arr, index, x);
    }

    auto callee    = c->as<App>();
    auto [T, r, s] = callee->args<3>();
    w.DLOG("    T = {} : {}", T, T->type());
    w.DLOG("    r = {} : {}", r, r->type());
    w.DLOG("    s = {} : {}", s, s->type());

    auto r_lit = r->isa<Lit>();
    if (!r_lit) return nullptr;

    auto r_nat     = r_lit->get<u64>();
    auto new_r_nat = r_nat - 1;

    auto idx = index->proj(r_nat, 0);
    w.DLOG("    idx = {} : {}", idx, idx->type());
    auto new_r     = w.lit_nat(new_r_nat);
    auto new_s_vec = DefVec(new_r_nat, [&](size_t i) { return s->proj(r_nat, i + 1); });
    auto new_s     = w.tuple(new_s_vec);

    auto target_arr    = w.extract(arr, idx);
    auto new_index_vec = DefVec(new_r_nat, [&](size_t i) { return index->proj(r_nat, i + 1); });
    auto new_index     = w.tuple(new_index_vec);
    auto new_arr       = op_set(T, new_r, new_s, target_arr, new_index, x);

    auto idx_n   = idx->type()->op(1);
    auto idx_lit = idx_n->isa<Lit>();
    if (!idx_lit) return nullptr;
    if (idx_lit->get<u64>() == 1) return new_arr;

    return w.insert(arr, idx, new_arr);
}

const Def* normalize_broadcast(const Def*, const Def* c, const Def* arg) {
    auto& w = c->world();

    auto [s_in, s_out, input] = arg->projs<3>();
    auto callee         = c->as<App>();
    auto [T, r]   = callee->args<2>();
    w.DLOG("normalize_broadcast");
    w.DLOG("    s_out = {} : {}", s_out, s_out->type());
    w.DLOG("    input = {} : {}", input, input->type());
    w.DLOG("    T = {} : {}", T, T->type());
    w.DLOG("    r = {} : {}", r, r->type());
    w.DLOG("    s_in = {} : {}", s_in, s_in->type());

    auto r_lit = r->isa<Lit>();
    if (!r_lit) return nullptr;
    auto r_nat     = r_lit->get<u64>();
    auto new_r_nat = r_nat - 1;

    if (new_r_nat == 0) {
        if (s_in == s_out) return input;
        auto s_in_lit = s_in->isa<Lit>();
        if (!s_in_lit) return nullptr;
        auto s_in_nat = s_in_lit->get<u64>();
        assert(s_in_nat == 1 && "input dimensions must be 1 or equal to the output dimension");
        return w.pack(s_out, input);
    }

    auto s_in_0   = s_in->proj(r_nat, 0);
    auto s_in_lit = s_in_0->isa<Lit>();
    if (!s_in_lit) return nullptr;
    auto s_in_nat = s_in_lit->get<u64>();

    auto s_out_0   = s_out->proj(r_nat, 0);
    auto s_out_lit = s_out_0->isa<Lit>();
    if (!s_out_lit) return nullptr;
    auto s_out_nat = s_out_lit->get<u64>();

    auto new_r         = w.lit_nat(new_r_nat);
    auto new_s_in_vec  = DefVec(new_r_nat, [&](size_t i) { return s_in->proj(r_nat, i + 1); });
    auto new_s_in      = w.tuple(new_s_in_vec);
    auto new_s_out_vec = DefVec(new_r_nat, [&](size_t i) { return s_out->proj(r_nat, i + 1); });
    auto new_s_out     = w.tuple(new_s_out_vec);

    auto bc = w.annex<tensor::broadcast>();
    bc      = w.app(bc, {T, new_r});
    if (s_in_0 == s_out_0) {
        auto out_vec = DefVec(s_out_nat, [&](size_t i) { return w.app(bc, {new_s_in, new_s_out, input->proj(i)}); });
        return w.tuple(out_vec);
    }
    assert(s_in_nat == 1 && "input dimensions must be 1 or equal to the output dimension");
    if (s_in_nat == 1) {
        auto out = w.app(bc, {new_s_in, new_s_out, input});
        return w.pack(s_out_0, out);
    }
    return nullptr;
}

const Def* normalize_broadcast_in_dim(const Def*, const Def* c, const Def* arg) {
    auto& w = c->world();

    auto [s_in, s_out, input, index]  = arg->projs<4>();
    auto callee                 = c->as<App>();
    auto [T, r_in, r_out] = callee->args<3>();
    w.DLOG("normalize_broadcast_in_dim");
    w.DLOG("    s_out = {} : {}", s_out, s_out->type());
    w.DLOG("    input = {} : {}", input, input->type());
    w.DLOG("    index = {} : {}", index, index->type());
    w.DLOG("    T = {} : {}", T, T->type());
    w.DLOG("    r_in = {} : {}", r_in, r_in->type());
    w.DLOG("    r_out = {} : {}", r_out, r_out->type());
    w.DLOG("    s_in = {} : {}", s_in, s_in->type());

    auto r_in_lit = r_in->isa<Lit>();
    if (!r_in_lit) return nullptr;
    auto r_in_nat  = r_in_lit->get<u64>();
    auto r_out_lit = r_out->isa<Lit>();
    if (!r_out_lit) return nullptr;
    auto r_out_nat = r_out_lit->get<u64>();

    auto s_tr_vec = DefVec(r_out_nat, [&](size_t i) {
        if (i < r_in_nat) return s_in->proj(r_in_nat, i);
        return w.lit_nat_1()->as<Def>();
    });
    auto s_tr     = w.tuple(s_tr_vec);

    std::set<u64> set_perm;
    std::map<u64, u64> map_perm;
    for (u64 i = 0; i < r_out_nat; ++i) set_perm.insert(i);
    for (u64 i = 0; i < r_in_nat; ++i) {
        auto idx     = index->proj(r_in_nat, i);
        auto idx_lit = Lit::isa(idx);
        if (!idx_lit) return nullptr;
        u64 idx_nat = *idx_lit;

        map_perm[idx_nat] = i;

        set_perm.erase(idx_nat);
    }
    u64 j = r_in_nat;
    for (auto i = set_perm.begin(); i != set_perm.end(); i++) {
        map_perm[*i] = j;
        j++;
    }
    auto permutation_vec = DefVec(r_out_nat, [&](size_t i) { return w.lit_idx(r_out_nat, map_perm[i]); });
    auto permutation     = w.tuple(permutation_vec);

    auto tr = w.annex<tensor::transpose>();
    tr      = w.app(tr, {T, r_out, s_tr});
    tr      = w.app(tr, {input, permutation});

    auto s_bc_vec = DefVec(r_out_nat, [&](size_t i) { return s_tr->proj(r_out_nat, map_perm.at(i)); });
    auto s_bc     = w.tuple(s_bc_vec);

    auto bc = w.annex<tensor::broadcast>();
    bc      = w.app(bc, {T, r_out});
    bc      = w.app(bc, {s_bc, s_out, tr});

    return bc;
}

std::pair<Lam*, const Def*>
counting_for(const Def* bound, const Def* acc, const Def* exit, const char* name = "for_body") {
    auto& w       = bound->world();
    auto acc_ty   = acc->type();
    auto body     = w.mut_con({/* iter */ w.type_i32(), /* acc */ acc_ty, /* return */ w.cn(acc_ty)})->set(name);
    auto for_loop = w.call<affine::For>(Defs{w.lit_i32(0), bound, w.lit_i32(1), acc, body, exit});
    return {body, for_loop};
}

const Def* normalize_map_reduce(const Def* type, const Def* c, const Def* inputs) {
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

    auto& w     = c->world();
    auto callee = c->as<App>();

    auto subs = callee->arg();

    auto [comb, zero] = callee->decurry()->args<2>();
    auto [TI, NI, SI] = callee->decurry()->decurry()->args<3>();
    auto S            = callee->decurry()->decurry()->decurry()->arg();
    auto [T, n]       = callee->decurry()->decurry()->decurry()->decurry()->args<2>();
    auto m            = callee->decurry()->decurry()->decurry()->decurry()->decurry()->arg();

    w.DLOG("type : {}", type);
    w.DLOG("meta variables:");
    w.DLOG("  n = {}", n);
    w.DLOG("  S = {}", S);
    w.DLOG("  T = {}", T);
    w.DLOG("  m = {}", m);
    w.DLOG("  NI = {} : {}", NI, NI->type());
    w.DLOG("  TI = {} : {}", TI, TI->type());
    w.DLOG("  SI = {} : {}", SI, SI->type());
    w.DLOG("arguments:");
    w.DLOG("  zero = {}", zero);
    w.DLOG("  comb = {} : {}", comb, comb->type());
    w.DLOG("  subs = {} : {}", subs, subs->type());
    w.DLOG("  inputs = {} : {}", inputs, inputs->type());

    // Our goal is to generate a call to a function that performs:
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

    absl::flat_hash_map<u64, const Def*> dims;         // idx ↦ nat (size bound = dimension)
    absl::flat_hash_map<u64, const Def*> raw_iterator; // idx ↦ I32
    absl::flat_hash_map<u64, const Def*> iterator;     // idx ↦ %Idx (S/NI#i)
    Vector<u64> out_indices;                           // output indices 0..n-1
    Vector<u64> in_indices;                            // input indices ≥ n

    Vector<const Def*> output_dims; // i<n ↦ nat (dimension S#i)
    Vector<DefVec> input_dims;      // i<m ↦ j<NI#i ↦ nat (dimension SI#i#j)
    Vector<u64> n_input;            // i<m ↦ nat (number of dimensions of SI#i)

    auto n_lit = n->isa<Lit>();
    auto m_lit = m->isa<Lit>();
    if (!n_lit || !m_lit) {
        w.DLOG("n or m is not a literal");
        return nullptr;
    }

    auto n_nat = n_lit->get<u64>(); // number of output dimensions (in S)
    auto m_nat = m_lit->get<u64>(); // number of input matrices

    // collect output dimensions
    w.DLOG("out dims (n) = {}", n_nat);
    for (u64 i = 0; i < n_nat; ++i) {
        auto dim = S->proj(n_nat, i);
        w.DLOG("dim {} = {}", i, dim);
        dims[i] = dim;
        output_dims.push_back(dim);
    }

    // collect other (input) dimensions
    w.DLOG("matrix count (m) = {}", m_nat);

    for (u64 i = 0; i < m_nat; ++i) {
        auto ni     = NI->proj(m_nat, i);
        auto ni_lit = Lit::isa(ni);
        if (!ni_lit) {
            w.DLOG("matrix {} has non-constant dimension count", i);
            return nullptr;
        }
        u64 ni_nat = *ni_lit;
        w.DLOG("  dims({i}) = {}", i, ni_nat);
        auto SI_i = SI->proj(m_nat, i);
        DefVec input_dims_i;
        for (u64 j = 0; j < ni_nat; ++j) {
            auto dim = SI_i->proj(ni_nat, j);
            w.DLOG("    dim {} {} = {}", i, j, dim);
            input_dims_i.push_back(dim);
        }
        input_dims.push_back(input_dims_i);
        n_input.push_back(ni_nat);
    }

    // extracts bounds for each index (in, out)
    for (u64 i = 0; i < m_nat; ++i) {
        w.DLOG("investigate {} / {}", i, m_nat);
        auto indices = subs->proj(m_nat, i);
        auto mat     = inputs->proj(m_nat, i);
        w.DLOG("  indices {} = {}", i, indices);
        w.DLOG("  matrix {} = {}", i, mat);
        for (u64 j = 0; j < n_input[i]; ++j) {
            auto idx     = indices->proj(n_input[i], j);
            auto idx_lit = Lit::isa(idx);
            if (!idx_lit) {
                w.DLOG("    index {} {} is not a literal", i, j);
                return nullptr;
            }
            u64 idx_nat = *idx_lit;
            auto dim    = input_dims[i][j];
            w.DLOG("      index {} = {}", j, idx);
            w.DLOG("        dim {} = {}", idx, dim);
            if (!dims.contains(idx_nat)) {
                dims[idx_nat] = dim;
                w.DLOG("        {} ↦ {}", idx_nat, dim);
            } else {
                auto prev_dim = dims[idx_nat];
                w.DLOG("        prev dim {} = {}", idx_nat, prev_dim);
                // override with more precise information
                if (auto dim_lit = dim->isa<Lit>()) {
                    if (auto prev_dim_lit = prev_dim->isa<Lit>()) {
                        if (dim != prev_dim) {
                            if (!dim_lit) return nullptr;
                            if (!prev_dim_lit) return nullptr;
                            assert(dim_lit->get<u64>() == prev_dim_lit->get<u64>() && "dimensions must be equal");
                        }
                    } else
                        dims[idx_nat] = dim;
                } else if (dim != prev_dim) {
                    w.DLOG("dimensions {} and {} must be equal", dim, prev_dim);
                    return nullptr;
                }
            }
        }
    }

    for (auto [idx, dim] : dims) {
        w.ILOG("dim {} = {}", idx, dim);
        if (idx < n_nat)
            out_indices.push_back(idx);
        else
            in_indices.push_back(idx);
    }
    // sort indices to make checks easier later.
    std::sort(out_indices.begin(), out_indices.end());
    std::sort(in_indices.begin(), in_indices.end());

    auto fun = w.mut_fun(w.sigma(), type)->set("mapRed");
    w.DLOG("fun {} : {}", fun, fun->type());

    auto ds_fun = direct::op_cps2ds_dep(fun)->set("dsFun");
    w.DLOG("ds_fun {} : {}", ds_fun, ds_fun->type());
    auto call = w.app(ds_fun, w.tuple())->set("call");
    w.DLOG("call {} : {}", call, call->type());

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
    auto init_mat = w.bot(type);
    w.DLOG("init_mat {} : {}", init_mat, init_mat->type());

    // The function on where to continue -- return after all output loops.
    auto cont        = fun->var(1);
    auto current_mut = fun;

    // Each of the outer loops contains the memory and matrix as accumulator (in an inner monad).
    auto acc = init_mat;

    for (auto idx : out_indices) {
        auto for_name    = w.sym("forIn_" + std::to_string(idx));
        auto dim_nat_def = dims[idx];
        auto dim         = w.call<core::bitcast>(w.type_i32(), dim_nat_def);
        w.DLOG("out_cont {} : {}", cont, cont->type());

        auto [body, for_call]       = counting_for(dim, acc, cont, for_name);
        auto [iter, new_acc, yield] = body->vars<3>();
        cont                        = yield;
        raw_iterator[idx]           = iter;
        iterator[idx]               = w.call<core::bitcast>(w.type_idx(dim_nat_def), iter);
        acc                         = new_acc;
        current_mut->set(true, for_call);
        current_mut = body;
    }

    // Now the inner loops for the inputs:
    // Each of the inner loops contains the element accumulator and memory as accumulator (in an inner monad).
    w.DLOG("acc at inner: {;}", acc);

    // First create the accumulator.
    auto element_acc = zero;
    element_acc->set("acc");
    auto wb_matrix = acc;
    assert(wb_matrix);
    w.DLOG("wb_matrix {} : {}", wb_matrix, wb_matrix->type());

    // Write back element to matrix. Set this as return after all inner loops.
    auto write_back = w.mut_con(T)->set("matrixWriteBack");
    w.DLOG("write_back {} : {}", write_back, write_back->type());
    auto element_final = write_back->var(0);

    auto output_iterators = DefVec((size_t)n_nat, [&](u64 i) {
        auto idx = out_indices[i];
        if (idx != i) w.ELOG("output indices must be consecutive 0..n-1 but {} != {}", idx, i);
        assert(idx == i && "output indices must be consecutive 0..n-1");
        auto iter_idx_def = iterator[idx];
        return iter_idx_def;
    });
    auto output_it_tuple  = w.tuple(output_iterators);
    w.DLOG("output tuple: {} : {}", output_it_tuple, output_it_tuple->type());

    auto written_matrix = op_set(T, n, S, wb_matrix, output_it_tuple, element_final);
    write_back->app(true, cont, {written_matrix});

    // From here on the continuations take the element and memory.
    acc  = element_acc;
    cont = write_back;

    for (auto idx : in_indices) {
        auto for_name    = w.sym("forIn_" + std::to_string(idx));
        auto dim_nat_def = dims[idx];
        auto dim         = w.call<core::bitcast>(w.type_i32(), dim_nat_def);
        w.DLOG("in_cont {} : {}", cont, cont->type());

        auto [body, for_call]       = counting_for(dim, acc, cont, for_name);
        auto [iter, new_acc, yield] = body->vars<3>();
        cont                        = yield;
        raw_iterator[idx]           = iter;
        iterator[idx]               = w.call<core::bitcast>(w.type_idx(dim_nat_def), iter);
        acc                         = new_acc;
        current_mut->set(true, for_call);
        current_mut = body;
    }
    element_acc = acc;

    // Read element from input matrix.
    DefVec input_elements((size_t)m_nat);
    for (u64 i = 0; i < m_nat; i++) {
        auto input_idx_tup = subs->proj(m_nat, i);
        auto input_matrix  = inputs->proj(m_nat, i);

        auto input_T = TI->proj(m_nat, i);
        auto input_N = NI->proj(m_nat, i);
        auto input_S = SI->proj(m_nat, i);

        if (m_nat == 1) input_S = SI;

        w.DLOG("input matrix {} is {} : {}", i, input_matrix, input_matrix->type());

        auto indices         = input_idx_tup->projs(n_input[i]);
        auto input_iterators = DefVec(n_input[i], [&](u64 j) {
            auto idx     = indices[j];
            auto idx_lit = idx->as<Lit>()->get<u64>();
            w.DLOG("  idx {} {} = {}", i, j, idx_lit);
            return iterator[idx_lit];
        });
        auto input_it_tuple  = w.tuple(input_iterators);

        auto read_entry = op_get(input_T, input_N, input_S, input_matrix, input_it_tuple);
        w.DLOG("read_entry {} : {}", read_entry, read_entry->type());
        auto element_i = read_entry->proj(0);
        input_elements[i] = element_i;
    }

    w.DLOG("  read elements {,}", input_elements);
    w.DLOG("  fun {} : {}", fun, fun->type());
    w.DLOG("  current_mut {} : {}", current_mut, current_mut->type());

    comb->set("comb");

    // TODO: make non-scalar or completely scalar?
    current_mut->app(true, comb, {w.tuple({element_acc, w.tuple(input_elements)}), cont});

    return call;
}

MIM_tensor_NORMALIZER_IMPL

} // namespace mim::plug::tensor
