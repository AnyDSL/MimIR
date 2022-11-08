#include "dialects/autodiff/auxiliary/mem/autodiff_mem_aux.h"

#include "dialects/affine/affine.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/direct/direct.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

const Def* shadow_array_type(const Def* def, const Def* arg_ty) {
    if (auto arr = def->isa<Arr>()) {
        auto& world = def->world();
        auto shape  = arr->shape();
        // TODO: does this need to be a deep structure?
        auto body = shadow_array_type(arr->body(), arg_ty);
        return world.arr(shape, body);
    }

    auto pb_ty = pullback_type(def, arg_ty);
    return pb_ty;
}

Lam* mem_return_lam(World& w, std::string name, Defs domain, bool filter) {
    auto mem_ty = mem::type_mem(w);
    // TODO: see comments from aux
    // auto cn     = build(w).add(mem_ty).add(domain).add(w.cn({mem_ty})).cn();
    auto cn  = w.cn({w.tuple({mem_ty, w.tuple(domain)}), w.cn({mem_ty})});
    auto lam = w.nom_lam(cn, w.dbg(name));
    lam->set_filter(filter);
    return lam;
}

// TODO: remove code duplication
Lam* return_lam(World& w, std::string name, Defs domain, bool filter) {
    auto mem_ty = mem::type_mem(w);
    // auto cn     = build(w).add(domain).add(w.cn({mem_ty})).cn();
    auto cn  = w.cn({w.tuple(domain), w.cn({mem_ty})});
    auto lam = w.nom_lam(cn, w.dbg(name));
    lam->set_filter(filter);
    return lam;
}

Lam* mem_lam(World& w, std::string name, bool filter) {
    auto mem_ty = mem::type_mem(w);
    auto lam    = w.nom_lam(w.cn({mem_ty}), w.dbg(name));
    lam->set_filter(filter);
    return lam;
}

Lam* call_pullback_ptr(const Def* gradient_ptr, const Def* pullback_ptr) {
    auto& w  = gradient_ptr->world();
    auto lam = mem_return_lam(w, "call_pullback", {}, false);

    auto [mem2, gradient_val] = mem::op_load(mem::mem_var(lam), gradient_ptr)->projs<2>();
    auto [mem3, pullback_val] = mem::op_load(mem2, pullback_ptr)->projs<2>();

    auto yield_ty = pullback_val->type()->as<Pi>()->dom(1)->as<Pi>();
    auto yield    = w.nom_lam(yield_ty, w.dbg("end_call_pullback"));
    yield->set_filter(false);

    yield->set_body(w.app(lam->var(1_s), mem::mem_var(yield)));
    auto arg = w.tuple({w.tuple({mem3, gradient_val}), yield});
    lam->set_body(w.app(pullback_val, arg));
    return lam;
}

Lam* call_pullback_arr(const Def* gradient_arr, const Def* pullback_arr) {
    auto& w = gradient_arr->world();

    auto ptr_ty = match<mem::Ptr>(gradient_arr->type());
    auto arr_ty = ptr_ty->arg(0)->isa<Arr>();
    auto shape  = arr_ty->shape();

    auto shape_lit = as_lit(shape);

    auto shape_i32 = core::op_bitcast(w.type_int(32), shape);

    auto loop = mem_return_lam(w, "call_pullback_arr", {}, false);
    auto brk  = mem_lam(w, "end_call_pullback_arr", false);
    auto body = return_lam(w, "body_call_pullback_arr", {w.type_int(32), mem::type_mem(w)}, false);
    auto next = body->var(2);

    brk->set_body(w.app(loop->var(1_s), mem::mem_var(brk)));

    auto i     = body->var(0_s);
    auto i_idx = core::op_bitcast(w.type_idx(shape_lit), i);

    auto gradient_lea = mem::op_lea(gradient_arr, i_idx);
    auto pullback_lea = mem::op_lea(pullback_arr, i_idx);

    Lam* caller = call_pullbacks(gradient_lea, pullback_lea);

    auto yield = mem_lam(w, "next_pullback_call", false);
    yield->set_filter(false);
    yield->set_body(w.app(next, mem::mem_var(yield)));

    body->set_body(w.app(caller, {mem::mem_var(body), yield}));

    auto app = affine::op_for(w, w.lit_int(32, 0), shape_i32, w.lit_int(32, 1), {mem::mem_var(loop)}, body, brk);
    loop->set_body(app);
    return loop;
}

Lam* call_pullbacks(const Def* gradient, const Def* pullback) {
    auto ptr     = match<mem::Ptr>(gradient->type());
    auto pointee = ptr->arg(0);
    if (pointee->isa<Arr>()) {
        return call_pullback_arr(gradient, pullback);
    } else {
        return call_pullback_ptr(gradient, pullback);
    }
}

const Lam* wrap_append_app(const Def* lam, const Def* call_after_lam) {
    auto& w = lam->world();

    auto lam_ty     = lam->type()->as<Pi>();
    auto lam_ret_ty = lam_ty->dom(1)->as<Pi>();

    auto wrapper = w.nom_lam(lam_ty, w.dbg(lam->name() + "_wrapper"));
    wrapper->set_filter(true);
    auto after_first = w.nom_lam(lam_ret_ty, w.dbg(lam->name() + "_after_lam"));
    after_first->set_filter(true);
    auto after_second = mem_lam(w, lam->name() + "_after_suffix");
    after_second->set_filter(true);

    auto arg = mem::replace_mem(mem::mem_var(after_second), after_first->var());
    after_second->set_body(w.app(wrapper->var(1_s), arg));
    after_first->set_body(w.app(call_after_lam, {mem::mem_var(after_first), after_second}));
    wrapper->set_body(w.app(lam, {wrapper->var(0_s), after_first}));
    return wrapper;
}

const Def* AutoDiffEval::buildAugmentedTuple(World& world, Defs aug_ops, const Pi* pb_ty, Lam* f, Lam*) {
    world.DLOG("buildAugmentedTuple of {,}", aug_ops);
    world.DLOG("buildAugmentedTuple pb_ty {}", pb_ty);
    auto aug_tup = world.tuple(aug_ops);

    DefArray pbs(aug_ops, [&](const Def* op) {
        auto pb = partial_pullback[op];
        if (!pb) {
            // pb = zero_pullback(op->type(), f_arg_ty);
            world.DLOG("No pullback for {} : {}", op, op->type());
            assert(pb && "pb should exists -- if not, create a zero_pb at correct position");
        }
        return pb;
    });
    world.DLOG("tuple pbs {,}", pbs);
    // We create the shadow pb (a shallow tuple of pullbacks).
    auto shadow_pb           = world.tuple(pbs);
    shadow_pullback[aug_tup] = shadow_pb;

    // ```
    // \lambda (s:[E0,...,Em]).
    //    sum (m,A)
    //      ((cps2ds e0*) (s#0), ..., (cps2ds em*) (s#m))
    // ```
    auto pb = world.nom_lam(pb_ty, world.dbg("tup_pb"));
    world.DLOG("Augmented tuple: {} : {}", aug_tup, aug_tup->type());
    world.DLOG("Tuple Pullback: {} : {}", pb, pb->type());
    world.DLOG("shadow pb: {} : {}", shadow_pb, shadow_pb->type());

    auto pb_tangent = pb->var((nat_t)0, world.dbg("tup_s"));

    DefArray tangents(pbs.size(),
                      [&](nat_t i) { return world.app(direct::op_cps2ds_dep(pbs[i]), world.extract(pb_tangent, i)); });
    pb->app(true, pb->var(1),
            // summed up tangents
            op_sum(tangent_type_fun(continuation_dom(f->type())), tangents));

    partial_pullback[aug_tup] = pb;

    return aug_tup;

// TODO: incorporate
#if 0
    auto T = tangent_type_fun(f_arg_ty);

    auto mem = mem::mem_var(pb);
    // auto T_without_mem = remove_mem(T);
    // TODO: here we explicitly want a lazy zero
    auto sum = autodiff_zero(mem, f);

    size_t src = 0;
    for (size_t i = 0; i < pbs.size(); i++) {
        auto re = direct::op_cps2ds_dep(pbs[i]);
        const Def* app;
        if (match<mem::M>(aug_ops[i]->type())) {
            app = world.app(re, {mem});
        } else {
            auto extract = world.extract(pb_tangent, src);
            while (match<mem::M>(extract->type())) {
                src++;
                extract = world.extract(pb_tangent, src);
            }
            src++;
            app = world.app(re, {mem, extract});
        }
        mem = world.extract(app, (nat_t)0);
        sum = world.app(world.app(world.ax<add>(), T), {sum, app});
    }

    sum = world.insert(sum, (u64)0, mem);

    // DefArray tangents(pbs.size(), [&](nat_t i) { return world.app(direct::op_cps2ds_dep(pbs[i]),
    // world.extract(pb_tangent, i)); });
    pb->app(true, pb->var(1), sum);
    partial_pullback[aug_tup] = pb;

    return aug_tup;
#endif
}

} // namespace thorin::autodiff
