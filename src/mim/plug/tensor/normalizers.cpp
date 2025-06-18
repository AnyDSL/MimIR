#include <cstddef>

#include <vector>

#include "mim/def.h"
#include "mim/world.h"

#include "mim/plug/affine/affine.h"
#include "mim/plug/core/core.h"
#include "mim/plug/direct/direct.h"
#include "mim/plug/tensor/autogen.h"
#include "mim/plug/tensor/tensor.h"

namespace mim::plug::tensor {

size_t get_size(const Def* x) {
    if (auto i = Lit::isa(x))
        return *i;
    else
        return x->num_ops();
}

const Def* op_get(const Def* arr, const Def* index) {
    return arr->world().bot(arr->proj(0)->type());

    auto n = index->num_ops();

    for (size_t i = 0; i < n; i++) {
        auto idx = Lit::isa(index->proj(i));
        arr      = arr->proj(*idx);
    }

    return arr;
}

const Def* op_set_rec(const Def* arr, std::vector<const Def*> index, const Def* x) {
    if (size(index) == 0) return x;

    if (auto idx = Lit::isa(index.back())) {
        index.pop_back();

        auto defs = DefVec();

        for (size_t i = 0; i < arr->num_ops(); i++)
            if (i == idx)
                defs.emplace_back(op_set_rec(arr->proj(i), index, x));
            else
                defs.emplace_back(arr->proj(i));
        return arr->world().tuple(defs);
    }
    return nullptr;
}

const Def* op_set(const Def* arr, const Def* index, const Def* x) {

    auto &w = arr->world();

    auto type = arr->unfold_type();

    w.DLOG("type: {}", type);

    auto f = w.app(w.annex<tensor::set>(), type);

    f = w.app(f, {w.lit_nat_1(), w.lit_nat_1()});

    f = w.app(f, {arr, index, x});

    return f;
    // auto& w = arr->world();
    // w.DLOG("op_set, arr: {}, index: {}, x: {}", arr->projs(), index->proj(0), x);

    // auto n = get_size(index);
    // std::vector<const Def*> unfold_n;
    // for (size_t i = 0; i < n - 1; i++) {
    //     auto proj_ = index->proj(i);
    //     unfold_n.push_back(proj_);
    // }
    // reverse(unfold_n.begin(), unfold_n.end());

    // return op_set_rec(arr, unfold_n, x);
}

std::pair<Lam*, const Def*> counting_for(const Def* bound, DefVec acc, const Def* exit, const char* name = "for_body") {
    auto& world = bound->world();
    auto acc_ty = world.tuple(acc)->type();
    auto body
        = world.mut_con({/* iter */ world.type_i32(), /* acc */ acc_ty, /* return */ world.cn(acc_ty)})->set(name);
    auto for_loop = affine::op_for(world, world.lit_i32(0), bound, world.lit_i32(1), acc, body, exit);
    return {body, for_loop};
}

const Def* normalize_map_reduce(const Def* type, const Def* c, const Def* is) {
    auto& w     = c->world();
    auto callee = c->as<App>();

    auto subs = callee->arg();

    auto [f, init] = callee->decurry()->args<2>();
    // auto [Tis, Ris, Sis] = callee->decurry()->decurry()->args<3>();
    auto So       = callee->decurry()->decurry()->decurry()->arg();
    auto [To, Ro] = callee->decurry()->decurry()->decurry()->decurry()->args<2>();
    auto nis      = callee->decurry()->decurry()->decurry()->decurry()->decurry()->arg();

    // if (auto is = is_Def->isa<Tuple>()) {
    // if (auto So = So_Def->isa<Tuple>()) {
    // auto n = is->num_ops();
    // auto n_out = So->num_ops();

    auto in_num    = Lit::isa(nis);
    nat_t out_dims = get_size(So);

    std::map<nat_t, const Def*> bounds, iters;
    w.DLOG("type: {}", type);
    w.DLOG("So: {}, projs: {}, ops: {}\n", So, So->projs(), So->ops());
    if (!(So->isa<Tuple>() || So->isa<Pack>() || So->isa<Lit>())) return nullptr;

    for (size_t i = 0; auto dim : So->projs()) bounds[i++] = dim;

    std::vector<std::vector<const Def*>> in_dims(*in_num);

    for (size_t i = 0; i < *in_num; i++) {
        auto S    = is->proj(i);
        auto dims = S->num_ops();
        for (size_t j = 0; j < dims; j++) {
            auto dim = S->proj(j);
            in_dims[i].push_back(dim);
            auto idx = Lit::isa(subs->proj(i)->proj(j));
            if (!idx) return nullptr;
            if (bounds.contains(*idx)) {
                // check if ‘bounds[*idx]‘ matches ‘dim‘ ...
                if (bounds[*idx]->num_ops() != dim->num_ops()) return nullptr;
            } else
                bounds[*idx] = dim;
        }
    }

    std::vector<nat_t> out_idxs, in_idxs;

    for (auto [idx, dim] : bounds) {
        w.DLOG("idx: {} dim: {}\n", idx, dim);
        if (idx < out_dims)
            out_idxs.push_back(idx); // outer loops
        else
            in_idxs.push_back(idx); // inner loops
    }

    std::ranges::sort(in_idxs), std::ranges::sort(out_idxs);

    auto map_red_fn = w.mut_lam(w.cn({w.sigma(), w.cn(type)}));
    auto Y          = w.bot(type); // placeholder
    DefVec curr_acc = {Y};
    auto curr_lam   = map_red_fn;
    auto next_con   = map_red_fn->var(1);
    for (auto idx : out_idxs) {
        auto for_name                = w.sym("forIn_" + std::to_string(idx));
        auto bound                   = bounds[idx];
        auto bound_i32               = w.call<core::bitcast>(w.type_i32(), bounds[idx]);
        auto [body, head]            = counting_for(bound_i32, curr_acc, next_con, for_name);
        auto [iter, next_acc, yield] = body->vars<3>();
        iters[idx]                   = w.call<core::bitcast>(w.type_idx(bound), iter);
        curr_acc                     = {next_acc->proj(0)};
        curr_lam->set(true, head), curr_lam = body;
        next_con = yield;
    }

    auto set_fn = w.mut_lam(w.cn(To));
    w.DLOG("set_fn {} : {}", set_fn, set_fn->type());
    Y           = curr_acc[0];
    auto y      = set_fn->var(0);
    auto y_acc  = init;
    DefVec out_idx(out_dims, [&](size_t i) {
        auto idx = out_idxs[i];
        w.DLOG("i: {}, idx: {}, iter: {}", i, idx, iters[idx]);
        return iters[idx];
    });
    w.DLOG("out_idx: {}\n", out_idx);
    auto wt = w.tuple(out_idx);
    w.DLOG("wt: {}\n", wt);
    Y = op_set(Y, w.tuple(out_idx), y);
    // Y = w.app(w.annex<tensor::set>(), {});
    // if (auto Y_tmp = op_set(Y, w.tuple(out_idx), y))
    //     Y = Y_tmp;
    // else
    //     return nullptr;
    set_fn->app(true, next_con, Y);
    curr_acc = {y_acc};
    next_con = set_fn;

    for (auto idx : in_idxs) {
        auto for_name                = w.sym("forIn_" + std::to_string(idx));
        auto bound                   = w.call<core::bitcast>(w.type_i32(), bounds[idx]);
        auto [body, head]            = counting_for(bound, curr_acc, next_con, for_name);
        auto [iter, next_acc, yield] = body->vars<3>();
        iters[idx]                   = w.call<core::bitcast>(w.type_idx(bounds[idx]), iter);
        curr_acc                     = {next_acc->proj(0)};
        curr_lam->set(true, head), curr_lam = body;
        next_con = yield;
    }

    DefVec xs(*in_num);
    for (size_t i = 0; i < *in_num; i++) {
        auto X = is->proj(i);
        DefVec in_idx(in_dims[i].size(), [&](size_t j) {
            auto idx = Lit::as(subs->proj(i)->proj(j));
            return iters[idx];
        });
        xs[i] = op_get(X, w.tuple(in_idx));
    }
    y_acc = curr_acc[0];
    w.DLOG("currlam: {}, type: {}", curr_lam, curr_lam->type());

    curr_lam->app(true, f, {w.tuple({y_acc, w.tuple(xs)}), next_con});
    return w.app(direct::op_cps2ds_dep(map_red_fn), w.tuple());

    // return nullptr;
}

MIM_tensor_NORMALIZER_IMPL

} // namespace mim::plug::tensor
