#include "mim/plug/tensor/phase/lower.h"

#include "mim/def.h"

#include "mim/plug/affine/affine.h"
#include "mim/plug/core/core.h"
#include "mim/plug/direct/direct.h"
#include "mim/plug/tensor/tensor.h"

namespace mim::plug::tensor::phase {

const Def* Lower::lower_get(const App* app) {
    auto& w  = app->world();
    auto c   = app->callee();
    auto arg = app->arg();

    auto [arr, index] = arg->projs<2>();
    auto callee       = c->as<App>();
    auto [T, r, s]    = callee->args<3>();

    arr   = rewrite(arr);
    index = rewrite(index);

    w.DLOG("lower_get");
    w.DLOG("    arr = {} : {}", arr, arr->type());
    w.DLOG("    arr shape = {}", arr->type()->as<Seq>()->arity());
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
    if (!r_lit) {
        w.WLOG("{} doesn't have a lowering-time known rank: {}", app, r);
        return nullptr;
    }
    auto r_nat    = r_lit->get<u64>();
    auto curr_arr = arr;
    for (auto ri = 0_u64; ri < r_nat; ++ri) {
        auto idx = index->proj(r_nat, ri);
        w.DLOG("    idx = {} : {}", idx, idx->type());
        curr_arr = w.extract(curr_arr, idx);
    }
    return curr_arr;
}

const Def* Lower::lower_set(const App* app) {
    auto& w  = app->world();
    auto c   = app->callee();
    auto arg = app->arg();

    auto [arr, index, x] = arg->projs<3>();

    arr   = rewrite(arr);
    index = rewrite(index);
    x     = rewrite(x);

    w.DLOG("lower_set");
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
    if (!r_lit) {
        w.WLOG("{} doesn't have a lowering-time known rank: {}", app, r);
        return nullptr;
    }

    auto r_nat = r_lit->get<u64>();
    DefVec arrs_to_insert_into(r_nat);
    arrs_to_insert_into[0] = arr;
    for (auto ri = 0_u64; ri < r_nat - 1; ++ri) {
        auto idx = index->proj(r_nat, ri);
        w.DLOG("    extract idx = {} : {}", idx, idx->type());
        arrs_to_insert_into[ri + 1] = w.extract(arrs_to_insert_into[ri], idx);
    }

    auto new_arr = x;
    for (auto ri = static_cast<s64>(r_nat - 1); ri >= 0; --ri) {
        auto idx = index->proj(r_nat, ri);
        w.DLOG("    idx = {} : {}", idx, idx->type());
        w.DLOG("    arr_to_insert_into = {} : {}", arrs_to_insert_into[ri], arrs_to_insert_into[ri]->type());

        new_arr = w.insert(arrs_to_insert_into[ri], idx, new_arr);
    }
    return new_arr;
}

const Def* Lower::rewrite_imm_App(const App* app) {
    if (auto get = Axm::isa<tensor::get>(app)) {
        if (auto res = lower_get(get)) return res;
    } else if (auto set = Axm::isa<tensor::set>(app)) {
        if (auto res = lower_set(set)) return res;
    }
    return Rewriter::rewrite_imm_App(app);
}

} // namespace mim::plug::tensor::phase
