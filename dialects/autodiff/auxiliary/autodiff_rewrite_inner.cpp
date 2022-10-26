#include <algorithm>
#include <string>

#include "thorin/tuple.h"

#include "thorin/util/assert.h"

#include "dialects/affine/affine.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/autogen.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

// TODO remove macro
#define f_arg_ty continuation_dom(f->type())

const Def* AutoDiffEval::augment_lit(const Lit* lit, Lam* f, Lam*) {
    auto pb               = zero_pullback(lit->type(), f_arg_ty);
    partial_pullback[lit] = pb;
    return lit;
}

const Def* AutoDiffEval::augment_var(const Var* var, Lam*, Lam*) {
    assert(augmented.count(var));
    auto aug_var = augmented[var];
    assert(partial_pullback.count(aug_var));
    return var;
}

const Def* AutoDiffEval::augment_lam(Lam* lam, Lam* f, Lam* f_diff) {
    auto& world = lam->world();
    // TODO: we need partial pullbacks for tuples (higher-order / ret-cont application)
    // also for higher-order args, ret_cont (at another point)
    // the pullback is not important but formally required by tuple rule
    if (augmented.count(lam)) {
        // We already know the function:
        // * recursion
        // * higher order arguments
        // * new encounter of previous function
        world.DLOG("already augmented {} : {} to {} : {}", lam, lam->type(), augmented[lam], augmented[lam]->type());
        return augmented[lam];
    }
    // TODO: better fix (another pass as analysis?)
    // TODO: handle open functions
    if (is_open_continuation(lam) || lam->name().find("ret") != std::string::npos ||
        lam->name().find("_cont") != std::string::npos || !is_closed(lam)) {
        // A open continuation behaves the same as return:
        // ```
        // cont: Cn[X]
        // cont': Cn[X,Cn[X,A]]
        // ```
        // There is dependency on the closed function context.
        // (All derivatives are with respect to the arguments of a closed function.)

        world.DLOG("found an open continuation {} : {}", lam, lam->type());
        // auto cont_dom = lam->type()->dom(); // not only 0 but all
        // auto pb_ty    = pullback_type(cont_dom, f_arg_ty);
        // auto aug_dom  = autodiff_inner_type_fun(cont_dom, f_arg_ty);
        // world.DLOG("augmented domain {}", aug_dom);
        // world.DLOG("pb type is {}", pb_ty);
        // auto aug_ty = world.cn({aug_dom, pb_ty});
        auto aug_ty = autodiff_inner_type_fun(lam->type(), f_arg_ty)->as<Pi>();
        world.DLOG("augmented type is {}", aug_ty);
        // assert(0);

        auto aug_lam = world.nom_lam(aug_ty, world.dbg("aug_" + lam->name()));
        open_continuation.insert(aug_lam);
        auto aug_var              = aug_lam->var((nat_t)0);
        augmented[lam->var()]     = aug_var;
        augmented[lam]            = aug_lam; // TODO: only one of these two
        derived[lam]              = aug_lam;
        auto pb                   = aug_lam->var(1);
        partial_pullback[aug_var] = pb;
        // We are still in same closed function.
        auto new_body = augment(lam->body(), f, f_diff);
        aug_lam->set_filter(lam->filter());
        // aug_lam->set_filter(false);
        aug_lam->set_body(new_body);

        auto lam_pb               = zero_pullback(lam->type(), f_arg_ty);
        partial_pullback[aug_lam] = lam_pb;
        world.DLOG("augmented {} : {}", lam, lam->type());
        world.DLOG("to {} : {}", aug_lam, aug_lam->type());
        world.DLOG("ppb for lam cont: {}", lam_pb);
        // assert(0);

        return aug_lam;
    }
    world.DLOG("found a closed function call {} : {}", lam, lam->type());
    // Some general function in the program needs to be differentiated.

    assert(is_closed(lam));

    auto aug_lam = op_autodiff(lam);
    // TODO: directly more association here? => partly inline op_autodiff
    world.DLOG("augmented function is {} : {}", aug_lam, aug_lam->type());
    return aug_lam;
}

const Def* AutoDiffEval::augment_extract(const Extract* ext, Lam* f, Lam* f_diff) {
    auto& world = ext->world();

    auto tuple = ext->tuple();
    auto index = ext->index();

    auto aug_tuple = augment(tuple, f, f_diff);
    auto aug_index = augment(index, f, f_diff);

    world.DLOG("tuple was: {} : {}", tuple, tuple->type());
    world.DLOG("aug tuple: {} : {}", aug_tuple, aug_tuple->type());
    auto aug_ext = world.extract(aug_tuple, aug_index);

    // TODO: check, but this case should be handled by shadow pullbacks (or id pb for argument which handles memory
    // correctly)
    // R if (match<mem::M>(ext->type())) {
    // R     partial_pullback[aug_ext] = zero_pullback_fun(ext->type(), f);
    // R     return aug_ext;
    // R }

    const Def* pb;
    if (shadow_pullback.count(aug_tuple)) {
        auto shadow_tuple_pb = shadow_pullback[aug_tuple];
        world.DLOG("Shadow pullback: {} : {}", shadow_tuple_pb, shadow_tuple_pb->type());
        pb = world.extract(shadow_tuple_pb, aug_index);
    } else {
        // ```
        // e:T, b:B
        // b = e#i
        // b* = \lambda (s:B). e* (insert s at i in (zero T))
        // ```
        assert(partial_pullback.count(aug_tuple));
        auto tuple_pb = partial_pullback[aug_tuple];
        auto pb_ty    = pullback_type(ext->type(), f_arg_ty);
        auto pb_fun   = world.nom_lam(pb_ty, world.dbg("extract_pb"));
        world.DLOG("Pullback: {} : {}", pb_fun, pb_fun->type());

        // auto aug_tuple_type = aug_tuple->type();

        auto mem = mem::mem_var(pb_fun);
        // auto pb_tangent_ty = mem::strip_mem_ty(pb_fun->dom(0));

        auto pb_tangent = mem::strip_mem(pb_fun->var((nat_t)0, world.dbg("s")));

        // TODO: should work with lazy zero (might even be more efficient as we only care about the insert index and
        // read(insert index _) is index)
        auto init = autodiff_zero(mem, aug_tuple);

        const Def* tuple_tan = world.insert(init, aug_index, pb_tangent, world.dbg("tup_s"));

        pb_fun->app(true, tuple_pb, {tuple_tan, pb_fun->var(1)});

        // world.DLOG("Tuple pb is {} : {}", tuple_pb, tuple_pb->type());
        // auto pb_tangent = pb_fun->var((nat_t)0, world.dbg("s"));
        // // R auto tuple_tan  = world.insert(op_zero(aug_tuple->type()), aug_index, pb_tangent, world.dbg("tup_s"));
        // //  we create a uni vector of E^T (not to be confused with (E')^T)
        // auto tuple_tan_type = tuple_pb->type()->as<Pi>()->dom(0);
        // auto tuple_tan      = world.insert(op_zero(tuple_tan_type), aug_index, pb_tangent, world.dbg("tup_s"));
        // world.DLOG("Unit Vector: {} : {}", tuple_tan, tuple_tan->type());
        // pb_fun->app(true, tuple_pb,
        //             {
        //                 tuple_tan,
        //                 pb_fun->var(1) // ret_var but make sure to select correct one
        //             });
        pb = pb_fun;
    }
    assert(pb);
    partial_pullback[aug_ext] = pb;

    return aug_ext;
}

const Def* AutoDiffEval::augment_tuple(const Tuple* tup, Lam* f, Lam* f_diff) {
    auto& world = tup->world();

    // augment ops

    auto projs = tup->projs();
    // TODO: should use ops instead?
    DefArray aug_ops(projs, [&](const Def* op) { return augment(op, f, f_diff); });

    auto aug_tup = world.tuple(aug_ops);

    DefArray pbs(aug_ops, [&](const Def* op) { return get_pullback(op, f); });
    world.DLOG("tuple pbs {,}", pbs);
    // create shadow pb
    auto shadow_pb           = world.tuple(pbs);
    shadow_pullback[aug_tup] = shadow_pb;

    // create partial pb
    // \lambda (s:[E0,...,Em]).
    //    sum (m,A)
    //      ((cps2ds e0*) (s#0), ..., (cps2ds em*) (s#m))
    auto pb_ty = pullback_type(tup->type(), f_arg_ty);
    auto pb    = world.nom_lam(pb_ty, world.dbg("tup_pb"));
    world.DLOG("Augmented tuple: {} : {}", aug_tup, aug_tup->type());
    world.DLOG("Tuple Pullback: {} : {}", pb, pb->type());
    world.DLOG("shadow pb: {} : {}", shadow_pb, shadow_pb->type());

    auto pb_tangent = pb->var((nat_t)0, world.dbg("tup_s"));

    // TODO: move op_cps2ds to direct dialect and merge then
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
}

const Def* AutoDiffEval::augment_pack(const Pack* pack, Lam* f, Lam* f_diff) {
    auto& world = pack->world();
    auto shape  = pack->arity(); // TODO: arity vs shape
    auto body   = pack->body();

    auto aug_shape = augment_(shape, f, f_diff);
    auto aug_body  = augment(body, f, f_diff);

    auto aug_pack = world.pack(aug_shape, aug_body);

    assert(partial_pullback[aug_body] && "pack pullback should exists");
    // TODO: or use scale axiom
    auto body_pb              = partial_pullback[aug_body];
    auto pb_pack              = world.pack(aug_shape, body_pb);
    shadow_pullback[aug_pack] = pb_pack;

    world.DLOG("shadow pb of pack: {} : {}", pb_pack, pb_pack->type());

    auto pb_type = pullback_type(pack->type(), f_arg_ty);
    auto pb      = world.nom_lam(pb_type, world.dbg("pack_pb"));

    world.DLOG("pb of pack: {} : {}", pb, pb_type);

    auto f_arg_ty_diff = tangent_type_fun(f_arg_ty);
    auto app_pb        = world.nom_pack(world.arr(aug_shape, f_arg_ty_diff));

    // TODO: special case for const width (special tuple)

    // <i:n, cps2ds body_pb (s#i)
    app_pb->set(world.raw_app(direct::op_cps2ds_dep(body_pb), world.extract(pb->var((nat_t)0), app_pb->var())));

    world.DLOG("app pb of pack: {} : {}", app_pb, app_pb->type());

    auto sumup = world.raw_app(world.ax<sum>(), {aug_shape, f_arg_ty_diff});
    world.DLOG("sumup: {} : {}", sumup, sumup->type());

    pb->app(true, pb->var(1), world.raw_app(sumup, app_pb));

    partial_pullback[aug_pack] = pb;

    return aug_pack;
}

Lam* mem_return_lam(World& w, std::string name, Defs domain = {}, bool filter = true) {
    auto mem_ty = mem::type_mem(w);
    // TODO: see comments from aux
    // auto cn     = build(w).add(mem_ty).add(domain).add(w.cn({mem_ty})).cn();
    auto cn  = w.cn({w.tuple({mem_ty, w.tuple(domain)}), w.cn({mem_ty})});
    auto lam = w.nom_lam(cn, w.dbg(name));
    lam->set_filter(filter);
    return lam;
}

Lam* return_lam(World& w, std::string name, Defs domain = {}, bool filter = true) {
    auto mem_ty = mem::type_mem(w);
    // auto cn     = build(w).add(domain).add(w.cn({mem_ty})).cn();
    auto cn  = w.cn({w.tuple(domain), w.cn({mem_ty})});
    auto lam = w.nom_lam(cn, w.dbg(name));
    lam->set_filter(filter);
    return lam;
}

Lam* mem_lam(World& w, std::string name, bool filter = true) {
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

Lam* call_pullbacks(const Def* gradient, const Def* pullback);
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

const Def* AutoDiffEval::wrap_call_pullbacks(const Def* arg_pb, const Def* arg) {
    auto& w = arg->world();

    DefVec pullbacks;
    for (auto arg_proj : arg->projs()) {
        auto augment_arg_proj = augmented[arg_proj];
        if (!augment_arg_proj) continue;
        auto pullback_root = shadow_pullback[augment_arg_proj];
        if (!pullback_root) continue;
        pullbacks.push_back(pullback_root);
    }

    auto propagate_gradients = w.nom_lam(arg_pb->type()->as<Pi>(), w.dbg("propagate_gradients"));
    propagate_gradients->set_filter(false);

    DefVec gradients;
    for (auto var : propagate_gradients->var(0_s)->projs()) {
        if (match<mem::Ptr>(var->type())) { gradients.push_back(var); }
    }

    auto pullbacks_size = pullbacks.size();
    auto gradients_size = gradients.size();

    assert(pullbacks_size == gradients_size);

    auto current = propagate_gradients;

    for (size_t i = 0; i < pullbacks_size; i++) {
        auto gradient = gradients[i];
        auto pullback = pullbacks[i];

        auto next     = mem_lam(w, "next_loop", false);
        auto loop_lam = call_pullbacks(gradient, pullback);
        current->set_body(w.app(loop_lam, {mem::mem_var(current), next}));
        current = next;
    }

    auto exit_arg = mem::replace_mem(mem::mem_var(current), propagate_gradients->var());
    current->set_body(w.app(arg_pb, exit_arg));
    return propagate_gradients;
}

Lam* AutoDiffEval::free_memory_lam() {
    auto& w = world();

    auto free = mem_return_lam(w, "free");

    auto mem = mem::mem_var(free);
    for (auto memory : allocated_memory) { mem = mem::op_free(mem, memory); }

    free->set_body(w.app(free->var(1_s), mem));
    return free;
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

const Def* AutoDiffEval::augment_app(const App* app, Lam* f, Lam* f_diff) {
    auto& world = app->world();

    auto callee = app->callee();
    auto arg    = app->arg();

    callee->type()->dump();
    arg->type()->dump();

    auto aug_arg    = augment(arg, f, f_diff);
    auto aug_callee = augment(callee, f, f_diff);
    // auto arg_ppb    = partial_pullback[aug_arg];

    world.DLOG("augmented argument <{}> {} : {}", aug_arg->unique_name(), aug_arg, aug_arg->type());
    world.DLOG("augmented callee  <{}> {} : {}", aug_callee->unique_name(), aug_callee, aug_callee->type());
    // TODO: move down to if(!is_cont(callee))
    if (!is_continuation(callee) && is_continuation(aug_callee)) {
        aug_callee = direct::op_cps2ds_dep(aug_callee);
        world.DLOG("wrapped augmented callee: <{}> {} : {}", aug_callee->unique_name(), aug_callee, aug_callee->type());
    }

    // nested (inner application)
    if (app->type()->isa<Pi>()) {
        world.DLOG("Nested application callee: {} : {}", aug_callee, aug_callee->type());
        world.DLOG("Nested application arg: {} : {}", aug_arg, aug_arg->type());
        auto aug_app = world.app(aug_callee, aug_arg);
        world.DLOG("Nested application result: <{}> {} : {}", aug_app->unique_name(), aug_app, aug_app->type());
        // TODO: rephrase
        // partial_pullback[aug_app] = partial_pullback[aug_callee];
        // or app with args
        // => nothing because we handle pullbacks of callees differently (in term level as they are closed)
        return aug_app;
    }

    // continuation (ret, if, ...)
    if (is_open_continuation(callee) || open_continuation.contains(aug_callee)) {
        // TODO: check if function (not operator)
        // original function = unclosed function (return cont / continuation)
        //   Cn[E]
        // aug_calle (looks like a function but is not really)
        //   Cn[E, Cn[E, Cn[A]]]

        // ret(e) => ret'(e, e*)

        world.DLOG("continuation {} : {} => {} : {}", callee, callee->type(), aug_callee, aug_callee->type());
        bool isa = aug_arg->isa<Extract>();
        if (!partial_pullback[aug_arg]) {
            augmented.erase(arg);
            augment(arg, f, f_diff);
        }
        auto arg_pb = partial_pullback[aug_arg];

        if (callee == f->ret_var()) {
            arg_pb = wrap_call_pullbacks(arg_pb, arg);
            arg_pb = wrap_append_app(arg_pb, free_memory_lam());
        }

        auto aug_app = world.app(aug_callee, {aug_arg, arg_pb});
        world.DLOG("Augmented application: {} : {}", aug_app, aug_app->type());
        return aug_app;
    }

    // ds function
    if (!is_continuation(callee)) {
        auto aug_app = world.app(aug_callee, aug_arg);
        world.DLOG("Augmented application: <{}> {} : {}", aug_app->unique_name(), aug_app, aug_app->type());

        world.DLOG("ds function: {} : {}", aug_app, aug_app->type());
        // calle is ds function (e.g. operator (or its partial application))
        auto [aug_res, fun_pb] = aug_app->projs<2>();
        // compose fun_pb with argument_pb to get result pb
        // TODO: combine case with cps function case
        auto arg_pb = partial_pullback[aug_arg];
        assert(arg_pb);
        // fun_pb: out_tan -> arg_tan
        // arg_pb: arg_tan -> fun_tan
        world.DLOG("function pullback: {} : {}", fun_pb, fun_pb->type());
        world.DLOG("argument pullback: {} : {}", arg_pb, arg_pb->type());
        auto res_pb = compose_continuation(arg_pb, fun_pb);
        world.DLOG("result pullback: {} : {}", res_pb, res_pb->type());
        partial_pullback[aug_res] = res_pb;
        // assert(0);
        world.debug_dump();
        // assert(0);
        // R assert(false);
        return aug_res;
    }

    // normal function app
    // g: cn[E, cn X]
    // g(args,cont)
    // g': cn[E, cn[X, cn[X, cn E]]]
    // g'(aug_args, ____)

    /*
    example:
    g: cn[E, cn X]
    g(e,ret_g):
        ret_g(x)
    f: cn[A, cn X]
    f(a,ret_f):
        g(e,ret_f)
        ^ this app

    g': cn[E, cn[X, cn[X, cn E]]]
    g'(e, ret_g'):
        ret_g'(aug_x, x*)
    f': cn[A, cn[X, cn[X, cn A]]]
        g'(aug_e, ?)

    TODO: compare with old approach

    idea (+ is ^t / ᵗ):
      1. we get the result rx from g
      2. forward it to ret (ret_f)
      3. get out_tangent x+
      4. convert it to e-tangent e+
      5. convert e tangent to a tangent a+
      6. return a tangent
    we have:
        ref_f': cn[X, cn[X+, cn A+]]
        ret_g': cn[X+, cn[X+, cn E+]]
        e*: cn[E+, cn A+]

    TODO: names
    define:
        (1,2)
        c1 : cn [X, cn [X+, cn E+]]
        does 1-6 tansitively
        takes result and pullback of result with respect to g arguments
        c1 := λ rx r*. ret_f' (rx, c2)

        (3,4)
        c2: cn [X+, cn A+]
        takes out tangent and acceptor of in tangent
        c2 := λ x+ fa+. r* (x+, c3)

        (4,5, 6)
        c3: cn E+
        takes e (g argument) tangent
        c3 := λ e+. e*(e+, fa+)
        (we used eta-conversion to fold fa+ directly into the application of e*)

    with reduction:
    we generalize ret_f' to cont:
        c1   : cn [X, cn [X+, cn E+]]
        rx   : X
        r*   : cn [X+, cn E+]
        cont : cn [X, cn [X+, cn A+]]
        e*   : cn [E+, cn A+]
        e*.r*: cn [X+, cn A+]
        c1 := λ rx r*. cont (rx, e* . r*)
        where . is composition

    */

    // TODO: dest with a function such that f args != g args

    // if(auto g_deriv=aug_callee->isa_nom<Lam>()){
    {
        auto g = callee;
        // at this point g_deriv might still be "autodiff ... g"
        auto g_deriv = aug_callee;
        world.DLOG("g: {} : {}", g, g->type());
        world.DLOG("g': {} : {}", g_deriv, g_deriv->type());

        auto [real_aug_args, aug_cont] = aug_arg->projs<2>();
        world.DLOG("real_aug_args: {} : {}", real_aug_args, real_aug_args->type());
        world.DLOG("aug_cont: {} : {}", aug_cont, aug_cont->type());
        auto e_pb = partial_pullback[real_aug_args];
        world.DLOG("e_pb (arg_pb): {} : {}", e_pb, e_pb->type());

        // TODO: better debug names
        auto ret_g_deriv_ty = g_deriv->type()->as<Pi>()->dom(1);
        world.DLOG("ret_g_deriv_ty: {} ", ret_g_deriv_ty);
        auto c1_ty = ret_g_deriv_ty->as<Pi>();
        world.DLOG("c1_ty: (cn[X, cn[X+, cn E+]]) {}", c1_ty);
        auto c1   = world.nom_lam(c1_ty, world.dbg("c1"));
        auto res  = c1->var((nat_t)0);
        auto r_pb = c1->var(1);
        c1->app(true, aug_cont, {res, compose_continuation(e_pb, r_pb)});

        auto aug_app = world.app(aug_callee, {real_aug_args, c1});
        world.DLOG("aug_app: {} : {}", aug_app, aug_app->type());

        // assert(0);
        return aug_app;
    }

    // result is * => no pb needed, no composition needed
    // TODO: compose pb (currently wrong)

    // TODO: handle cascading functions
    // TODO: handle axiom app before or after augment

    // return def;

    assert(false && "should not be reached");
}

const Def* AutoDiffEval::augment_lea(const App* lea, Lam* f, Lam* f_diff) {
    auto& w = world();

    auto [arr_ptr, idx] = lea->arg()->projs<2>();
    auto aug_ptr        = augment(arr_ptr, f, f_diff);
    auto aug_idx        = augment(idx, f, f_diff);

    auto aug_lea              = mem::op_lea(aug_ptr, aug_idx);
    partial_pullback[aug_lea] = zero_pullback_fun(aug_lea->type(), f);

    auto gradient_array = gradient_ptrs[aug_ptr];
    // no pullbacks, just need shadow information for ptr
    if (gradient_array) {
        gradient_ptrs[aug_lea] = mem::op_lea(gradient_array, aug_idx, w.dbg("pullback_lea"));
    } else {
        auto pullback_array = shadow_pullback[aug_ptr];
        assert(pullback_array);
        shadow_pullback[aug_lea] = mem::op_lea(pullback_array, aug_idx, w.dbg("pullback_lea"));
    }

    return aug_lea;
}

// TODO: rename to make connection to load clear
Lam* AutoDiffEval::create_gradient_collector(const Def* gradient_lea, Lam* f) {
    auto& w               = world();
    auto elem_ty          = match<mem::Ptr>(gradient_lea->type())->arg(0);
    auto [arg_ty, ret_pi] = f->type()->doms<2>();
    auto pb_type          = pullback_type(elem_ty, arg_ty);

    auto pb_lam = w.nom_lam(pb_type, w.dbg("load_pb"));

    auto [pb_arg, pb_ret] = pb_lam->vars<2>();
    auto [pb_mem, pb_s]   = pb_arg->projs<2>();

    auto [gradient_mem, gradient] = mem::op_load(pb_mem, gradient_lea, w.dbg("gradient_array_load"))->projs<2>();
    // TODO: use generalized add (maybe even on ptr?)
    auto add       = core::op(core::wrap::add, core::WMode::none, gradient, pb_s);
    auto store_mem = mem::op_store(gradient_mem, gradient_lea, add, w.dbg("add_to_gradient"));

    // TODO: split more make memory flow clear
    auto default_zero = autodiff_zero(store_mem, f);
    pb_lam->set_body(w.app(pb_ret, {default_zero}));
    pb_lam->set_filter(true);

    return pb_lam;
}

const Def* AutoDiffEval::augment_load(const App* load, Lam* f, Lam* f_diff) {
    auto& w = world();

    auto [load_mem, load_ptr] = load->args<2>();

    auto aug_mem = augment(load_mem, f, f_diff);
    auto aug_ptr = augment(load_ptr, f, f_diff);

    auto aug_load = mem::op_load(aug_mem, aug_ptr, w.dbg("aug_load"))->as<App>();

    auto aug_load_mem = aug_load->arg(1);

    auto gradient_ptr = gradient_ptrs[aug_ptr];
    if (gradient_ptr) {
        // for arrays with gradient array instead of loading pullbacks
        // we can specify a continuation which will add the gradient to the gradient array
        partial_pullback[aug_load] = create_gradient_collector(gradient_ptr, f);
        return aug_load;
    } else {
        // load pullback from shadow array
        auto pullback_ptr = shadow_pullback[aug_ptr];
        assert(pullback_ptr);
        auto [pullback_mem, pullback] = mem::op_load(aug_load_mem, pullback_ptr, w.dbg("pullback_load"))->projs<2>();
        partial_pullback[aug_load]    = pullback;
        return w.tuple({pullback_mem, aug_load});
    }
}

const Def* AutoDiffEval::augment_store(const App* store, Lam* f, Lam* f_diff) {
    auto& world = store->world();

    auto aug_arg                     = augment(store->arg(), f, f_diff);
    auto [aug_mem, aug_ptr, aug_val] = aug_arg->projs<3>();

    auto shadow_pb_ptr = shadow_pullback[aug_ptr];

    auto pb        = partial_pullback[aug_val];
    auto pb_store  = mem::op_store(aug_mem, shadow_pb_ptr, pb);
    auto aug_store = mem::op_store(pb_store, aug_ptr, aug_val);
    return aug_store;
}

const Def* AutoDiffEval::zero_pullback_fun(const Def* domain, Lam* f) {
    const Def* A   = f_arg_ty;
    auto& world    = A->world();
    auto A_tangent = tangent_type_fun(A);
    auto pb_ty     = pullback_type(domain, A);
    auto pb        = world.nom_lam(pb_ty, world.dbg("zero_pb"));
    // TODO: use lazy zero to delay execution as long as possible and allow for shortcut evaluation
    pb->app(true, pb->var(1), autodiff_zero(mem::mem_var(pb), f));
    return pb;
}

const Def* AutoDiffEval::get_pullback(const Def* op, Lam* f) {
    auto pb = partial_pullback[op];
    if (!pb) { return zero_pullback_fun(op->type(), f); }
    return pb;
}

const Def* AutoDiffEval::augment_alloc(const App* alloc, Lam* f, Lam* f_diff) {
    auto& world  = alloc->world();
    auto aug_mem = augment(alloc->arg(0_s), f, f_diff);

    auto callee = alloc->callee()->as<App>();
    auto type   = callee->arg(0_s);

    auto [alloc_mem, alloc_ptr] = mem::op_alloc(type, aug_mem)->projs<2>();

    auto pb_ty = shadow_array(type, f->dom(0_s));
    auto [alloc_mem_2, pullback_ptr] =
        mem::op_malloc(pb_ty, alloc_mem, world.dbg(alloc->name() + "_pullback_alloc_arr"))->projs<2>();

    allocated_memory.insert(pullback_ptr);
    shadow_pullback[alloc_ptr] = pullback_ptr;

    auto tup = world.tuple({alloc_mem_2, alloc_ptr});

    partial_pullback[tup]          = zero_pullback_fun(tup->type(), f);
    partial_pullback[alloc_mem_2]  = zero_pullback_fun(alloc_mem_2->type(), f);
    partial_pullback[pullback_ptr] = zero_pullback_fun(pullback_ptr->type(), f);

    return tup;
}

const Def* AutoDiffEval::augment_malloc(const App* malloc, Lam* f, Lam* f_diff) {
    auto aug_arg = augment(malloc->arg(), f, f_diff);
    // TODO: not yet implemented
    malloc->dump();
    malloc->dump();
    return malloc;
}

const Def* AutoDiffEval::augment_bitcast(const App* bitcast, Lam* f, Lam* f_diff) {
    auto& world  = bitcast->world();
    auto aug_arg = augment(bitcast->arg(), f, f_diff);

    auto dst              = core::op_bitcast(bitcast->type(), aug_arg);
    partial_pullback[dst] = zero_pullback_fun(dst->type(), f);
    return dst;
}

/// rewrite the given definition
///
const Def* AutoDiffEval::augment_(const Def* def, Lam* f, Lam* f_diff) {
    auto& world = def->world();
    // for types holds:
    // we use macros above to avoid recomputation
    // TODO: alternative: use class instance to rewrite inside a function and save such values (f, f_diff, f_arg_ty)

    world.DLOG("Augment def {} : {}", def, def->type());

    if (auto lea = match<mem::lea>(def)) { return augment_lea(lea->as<App>(), f, f_diff); }
    if (auto load = match<mem::load>(def)) { return augment_load(load->as<App>(), f, f_diff); }
    if (auto store = match<mem::store>(def)) { return augment_store(store->as<App>(), f, f_diff); }
    if (auto malloc = match<mem::malloc>(def)) { return augment_malloc(malloc->as<App>(), f, f_diff); }
    if (auto alloc = match<mem::alloc>(def)) { return augment_alloc(alloc->as<App>(), f, f_diff); }
    if (auto bitcast = match<core::bitcast>(def)) { return augment_bitcast(bitcast->as<App>(), f, f_diff); }

    // app => cont, operator, function
    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto arg    = app->arg();
        world.DLOG("Augment application: app {} with {}", callee, arg);
        return augment_app(app, f, f_diff);
    } else if (auto ext = def->isa<Extract>()) {
        auto tuple = ext->tuple();
        auto index = ext->index();
        world.DLOG("Augment extract: {} #[{}]", tuple, index);
        return augment_extract(ext, f, f_diff);
    } else if (auto var = def->isa<Var>()) {
        world.DLOG("Augment variable: {}", var);
        return augment_var(var, f, f_diff);
    } else if (auto lam = def->isa_nom<Lam>()) {
        world.DLOG("Augment nom lambda: {}", lam);
        return augment_lam(lam, f, f_diff);
    } else if (auto lam = def->isa<Lam>()) {
        world.ELOG("Augment lambda: {}", lam);
        assert(false && "can not handle non-nominal lambdas");
    } else if (auto lit = def->isa<Lit>()) {
        world.DLOG("Augment literal: {}", def);
        return augment_lit(lit, f, f_diff);
    } else if (auto tup = def->isa<Tuple>()) {
        world.DLOG("Augment tuple: {}", def);
        return augment_tuple(tup, f, f_diff);
    } else if (auto pack = def->isa<Pack>()) {
        // TODO: handle nom packs (dependencies in the pack) (=> see paper about vectors)
        auto shape = pack->arity(); // TODO: arity vs shape
        auto body  = pack->body();
        world.DLOG("Augment pack: {} : {} with {}", shape, shape->type(), body);
        return augment_pack(pack, f, f_diff);
    } else if (auto ax = def->isa<Axiom>()) {
        //  TODO: move concrete handling to own function / file / directory (file per dialect)
        world.DLOG("Augment axiom: {} : {}", ax, ax->type());
        world.DLOG("axiom curry: {}", ax->curry());
        world.DLOG("axiom flags: {}", ax->flags());
        std::string diff_name = ax->name();
        findAndReplaceAll(diff_name, ".", "_");
        findAndReplaceAll(diff_name, "%", "");
        diff_name = "internal_diff_" + diff_name;
        world.DLOG("axiom name: {}", ax->name());
        world.DLOG("axiom function name: {}", diff_name);

        auto diff_fun = world.lookup(diff_name);
        if (!diff_fun) {
            world.ELOG("derivation not found: {}", diff_name);
            auto expected_type = autodiff_type_fun(ax->type());
            world.ELOG("expected: {} : {}", diff_name, expected_type);
            assert(false && "unhandled axiom");
        }
        // TODO: why does this cause a depth error?
        // if (auto diff_lam = diff_fun->isa_nom<Lam>()) { diff_lam->set_filter(true); }
        return diff_fun;
    }

    // TODO: handle Pi for axiom app
    // TODO: remaining (lambda, axiom)

    world.ELOG("did not expect to augment: {} : {}", def, def->type());
    world.ELOG("node: {}", def->node_name());
    assert(false && "augment not implemented on this def");
}

} // namespace thorin::autodiff
