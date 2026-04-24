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

std::pair<Lam*, const Def*> counting_for(const Def* bound, const Def* acc, const Def* exit, Sym name) {
    auto& w       = bound->world();
    auto acc_ty   = acc->type();
    auto body     = w.mut_con({/* iter */ w.type_i32(), /* acc */ acc_ty, /* return */ w.cn(acc_ty)})->set(name);
    auto for_loop = w.call<affine::For>(body, exit, Defs{w.lit_i32(0), bound, w.lit_i32(1), acc});
    return {body, for_loop};
}

const Def* rec_broadcast(const Def* s_in, const Def* s_out, const Def* input, u64 r, u64 i, const Def* T) {
    auto& w      = s_in->world();
    auto s_in_ri = s_in->proj(r, i), s_out_ri = s_out->proj(r, i);
    w.DLOG("rec_broadcast");
    w.DLOG("    r = {}", r);
    w.DLOG("    i = {}", i);
    w.DLOG("    s_in_ri = {} : {}", s_in_ri, s_in_ri->type());
    w.DLOG("    s_out_ri = {} : {}", s_out_ri, s_out_ri->type());
    w.DLOG("    input = {} : {}", input, input->type());

    if (i + 1 == r) return input;

    if (s_in_ri == s_out_ri) {
        if (auto s_in_lit = s_in_ri->isa<Lit>()) {
            DefVec inputs(s_in_lit->get<u64>(),
                          [&](size_t j) { return rec_broadcast(s_in, s_out, input->proj(j), r, i + 1, T); });
            return w.tuple(inputs);
        } else {
            // TODO: we could probably support non-literal sizes as well, but we would need to generate loops to copy
            // the data instead of just packing it.
            w.WLOG("dimension {} of the input and output are equal but not literal: {} : {}", i, s_in_ri,
                   s_in_ri->type());
            return nullptr;
        }
    }

    if (auto s_in_lit = s_in_ri->isa<Lit>(); s_in_lit && s_in_lit->get<u64>() == 1) {
        w.DLOG("dimension {} of the input is 1, can be broadcasted to dimension {} of the output", i, s_out_ri);
        return w.pack(s_out_ri, rec_broadcast(s_in, s_out, input, r, i + 1, T));
    }

    w.WLOG("cannot broadcast dimension {} of size {} to size {}", i, s_in_ri, s_out_ri);
    return nullptr;
}

const Def* Lower::lower_broadcast(const App* app) {
    auto& w  = app->world();
    auto c   = app->callee();
    auto arg = app->arg();

    auto [s_in, s_out, input] = arg->projs<3>();
    auto callee               = c->as<App>();
    auto [T, r]               = callee->args<2>();
    w.DLOG("lower_broadcast");
    w.DLOG("    s_out = {} : {}", s_out, s_out->type());
    w.DLOG("    input = {} : {}", input, input->type());
    w.DLOG("    T = {} : {}", T, T->type());
    w.DLOG("    r = {} : {}", r, r->type());
    w.DLOG("    s_in = {} : {}", s_in, s_in->type());

    auto r_lit = r->isa<Lit>();
    if (!r_lit) {
        w.WLOG("{} doesn't have a lowering-time known rank: {}", app, r);
        return nullptr;
    }
    auto r_nat = r_lit->get<u64>();

    if (s_in == s_out) return input;

    if (r_nat == 1) {
        if (auto s_in_lit = s_in->isa<Lit>()) {
            auto s_in_nat = s_in_lit->get<u64>();
            assert(s_in_nat == 1 && "input dimensions must be 1 or equal to the output dimension");
            return w.pack(s_out, input);
        }
    }

    auto s_in_vec  = DefVec(r_nat, [&](size_t i) { return s_in->proj(r_nat, i); });
    auto s_out_vec = DefVec(r_nat, [&](size_t i) { return s_out->proj(r_nat, i); });

    auto result = rec_broadcast(s_in, s_out, input, r_nat, 0, T);
    w.DLOG("result of rec_broadcast = {} : {}", result, result->type());
    return result;
}

const Def* Lower::rewrite_imm_App(const App* app) {
    if (auto get = Axm::isa<tensor::get>(app)) {
        if (auto res = lower_get(get)) return res;
    } else if (auto set = Axm::isa<tensor::set>(app)) {
        if (auto res = lower_set(set)) return res;
    } else if (auto bc = Axm::isa<tensor::broadcast>(app)) {
        if (auto res = lower_broadcast(bc)) return res;
    }
    return Rewriter::rewrite_imm_App(app);
}

} // namespace mim::plug::tensor::phase
