#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/core/core.h"
#include "dialects/direct/direct.h"

namespace thorin::autodiff {

const Def* op_cps2ds(const Def* f) {
    auto& world = f->world();
    // TODO: assert continuation
    auto f_ty = f->type()->as<Pi>();
    auto T    = f_ty->dom(0);
    auto U    = f_ty->dom(1)->as<Pi>()->dom();
    // TODO: check if app can be used instead of raw_app
    return world.raw_app(world.raw_app(world.ax<direct::cps2ds>(), {T, U}), f);
}

const Def* op_sum(DefArray defs) {
    // TODO: assert all are of type T
    auto& world = defs[0]->world();
    auto T      = defs[0]->type();
    return world.raw_app(world.raw_app(world.ax<sum>(), {world.lit_nat(defs.size()), T}), world.tuple(defs));
}

#define f_arg_ty f->type()->dom(0)

const Def* AutoDiffEval::augment_lit(const Lit* lit, Lam* f, Lam* f_diff) {
    auto& world = lit->world();

    auto aug_lit = lit;
    // set zero pullback
    auto pb                   = zero_pullback(lit->type(), f_arg_ty);
    partial_pullback[aug_lit] = pb;
    return lit;
}

const Def* AutoDiffEval::augment_var(const Var* var, Lam* f, Lam* f_diff) {
    auto& world = var->world();
    assert(augmented.count(var));
    auto aug_var = augmented[var];
    assert(partial_pullback.count(aug_var));
    return var;
}

const Def* AutoDiffEval::augment_extract(const Extract* ext, Lam* f, Lam* f_diff) {
    auto& world = ext->world();

    auto tuple = ext->tuple();
    auto index = ext->index();

    auto aug_tuple = augment(tuple, f, f_diff);
    auto aug_index = augment(index, f, f_diff);

    // TODO: if not exists use:
    // e:T, b:B
    // b = e#i
    // b* = \lambda (s:B). e* (insert s at i in (zero T))

    const Def* pb;
    world.DLOG("tuple was: {} : {}", aug_tuple, aug_tuple->type());
    if (shadow_pullback.count(aug_tuple)) {
        auto shadow_tuple_pb = shadow_pullback[aug_tuple];
        world.DLOG("Shadow pullback: {} : {}", shadow_tuple_pb, shadow_tuple_pb->type());
        pb = world.extract(shadow_tuple_pb, aug_index);
    } else {
        assert(partial_pullback.count(aug_tuple));
        auto tuple_pb = partial_pullback[aug_tuple];
        auto pb_ty    = pullback_type(ext->type(), f_arg_ty);
        auto pb_fun   = world.nom_lam(pb_ty, world.dbg("extract_pb"));
        world.DLOG("Pullback: {} : {}", pb_fun, pb_fun->type());
        auto pb_tangent = pb_fun->var((nat_t)0, world.dbg("s"));
        auto tuple_tan  = world.insert(op_zero(aug_tuple->type()), aug_index, pb_tangent, world.dbg("tup_s"));
        pb_fun->app(true, tuple_pb,
                    {
                        tuple_tan,
                        pb_fun->var(1) // ret_var but make sure to select correct one
                    });
        pb = pb_fun;
    }

    auto aug_ext              = world.extract(aug_tuple, aug_index);
    partial_pullback[aug_ext] = pb;

    return aug_ext;
}

const Def* AutoDiffEval::augment_tuple(const Tuple* tup, Lam* f, Lam* f_diff) {
    auto& world = tup->world();

    // augment ops

    // TODO: should use ops instead?
    DefArray aug_ops(tup->projs(), [&](const Def* op) { return augment(op, f, f_diff); });
    auto aug_tup = world.tuple(aug_ops);

    DefArray pbs(aug_ops, [&](const Def* op) { return partial_pullback[op]; });
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

    auto pb_tangent = pb->var((nat_t)0, world.dbg("tup_s"));

    // TODO: move op_cps2ds to direct dialect and merge then
    DefArray tangents(pbs.size(), [&](nat_t i) { return world.app(op_cps2ds(pbs[i]), world.extract(pb_tangent, i)); });
    pb->app(true, pb->var(1),
            // summed up tangents
            op_sum(tangents));
    partial_pullback[aug_tup] = pb;

    return aug_tup;
}

const Def* AutoDiffEval::augment_app(const App* app, Lam* f, Lam* f_diff) {
    auto& world = app->world();

    auto callee = app->callee();
    auto arg    = app->arg();

    auto aug_arg    = augment(arg, f, f_diff);
    auto aug_callee = augment(callee, f, f_diff);
    // auto arg_ppb    = partial_pullback[aug_arg];

    // nested (inner application)
    if(app->type()->isa<Pi>()) {
        world.DLOG("Nested application: {} : {}", aug_callee, aug_callee->type());
        world.DLOG("Nested application arg: {} : {}", aug_arg, aug_arg->type());
        return world.app(aug_callee, aug_arg);
    }

    if (is_open_continuation(callee)) {
        // TODO: check if function (not operator)
        // original function = unclosed function (return cont / continuation)
        //   Cn[E]
        // aug_calle (looks like a function but is not really)
        //   Cn[E, Cn[E, Cn[A]]]

        // ret(e) => ret'(e, e*)

        world.DLOG("continuation {} : {} => {} : {}", callee, callee->type(), aug_callee, aug_callee->type());

        auto arg_pb  = partial_pullback[aug_arg];
        auto aug_app = world.app(aug_callee, {aug_arg, arg_pb});
        world.DLOG("Augmented application: {} : {}", aug_app, aug_app->type());
        // assert(false);
        return aug_app;
    }

    if(!is_continuation(callee)) {
        // calle is ds function (e.g. operator (or its partial application))
        auto aug_app = world.app(aug_callee, aug_arg);
        auto [aug_res,fun_pb] = aug_app->projs<2>();
        // compose fun_pb with argument_pb to get result pb
        // TODO: combine case with cps function case
        auto arg_pb = partial_pullback[aug_arg];
        assert(arg_pb);
        // fun_pb: out_tan -> arg_tan
        // arg_pb: arg_tan -> fun_tan
        world.DLOG("argument pullback: {} : {}", arg_pb, arg_pb->type());
        world.DLOG("function pullback: {} : {}", fun_pb, fun_pb->type());
        auto res_pb = compose_continuation(arg_pb,fun_pb);
        world.DLOG("result pullback: {} : {}", res_pb, res_pb->type());
        partial_pullback[aug_res] = res_pb;
        //R assert(false);
        return aug_res;
    }

    // TODO: handle cascading functions
    // TODO: handle axiom app before or after augment

    // return def;

    assert(false);
}

/// rewrite the given definition
///
const Def* AutoDiffEval::augment_(const Def* def, Lam* f, Lam* f_diff) {
    auto& world = def->world();
    // for types holds:
    // we use macros above to avoid recomputation
    // TODO: alternative: use class instance to rewrite inside a function and save such values (f, f_diff, f_arg_ty)
    // f_arg_ty = f->type()->dom(0);
    // f_arg_tangent_ty = tangent_type_fun(f_arg_ty);
    // f_arg_tangent_ty =
    //     f_diff->type()->dom(1)->as<Pi>() // return type
    //     ->dom(1)->as<Pi>() // pb type
    //     ->dom(1)->as<Pi>()->dom(); // arg tangent type

    world.DLOG("Augment def {} : {}", def, def->type());

    // app => cont, operator, function
    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto arg    = app->arg();
        world.DLOG("Augment application: app {} with {}", callee, arg);
        return augment_app(app, f, f_diff);
    }

    // projection
    else if (auto ext = def->isa<Extract>()) {
        // world.DLOG("Augment extract: {}", def);
        auto tuple = ext->tuple();
        auto index = ext->index();
        world.DLOG("Augment extract: {} #[{}]", tuple, index);
        return augment_extract(ext, f, f_diff);
    }

    // vars (function argument)
    else if (auto var = def->isa<Var>()) {
        world.DLOG("Augment variable: {}", var);
        return augment_var(var, f, f_diff);
    }

    // constants
    else if (auto lit = def->isa<Lit>()) {
        world.DLOG("Augment literal: {}", def);
        return augment_lit(lit, f, f_diff);
    }

    // tuple
    else if (auto tup = def->isa<Tuple>()) {
        world.DLOG("Augment tuple: {}", def);
        return augment_tuple(tup, f, f_diff);
    }

    // axiom
    //  TODO: move concrete handling to own file, directory
    else if (auto ax = def->isa<Axiom>()) {
        world.DLOG("Augment axiom: {} : {}", ax, ax->type());
        world.DLOG("axiom curry: {}", ax->curry());
        world.DLOG("axiom flags: {}", ax->flags());
        // return augment_axiom(ax, f, f_diff);

        if (ax->flags() == core::wrap::mul) {
            world.DLOG("multiplication axiom flags");

            auto mul_deriv_cps = world.lookup("mul_deriv_cps");
            auto mul_deriv_ds  = world.lookup("mul_deriv_ds");
            if (!mul_deriv_cps) {
                world.ELOG("multiplication derivative in cps not found");
            } else {
                world.DLOG("mul deriv cps: {} : {}", mul_deriv_cps, mul_deriv_cps->type());
            }
            if (!mul_deriv_ds) {
                world.ELOG("multiplication derivative in ds not found");
            } else {
                world.DLOG("mul deriv ds: {} : {}", mul_deriv_ds, mul_deriv_ds->type());
            }
            // we use ds => 
            // inlining already needed for \Pi arguments => should(/needs to) work
            // no need for cps2ds axiom insertion in the transformation after \Pi arguments
            // handling of the difference between ds operations and cps functions 
            //   already needed for functions vs operators
            auto mul_deriv_ds2  = world.lookup("mul_deriv_ds_by_cps");
            assert(mul_deriv_ds2);


            world.DLOG("filter of mul_deriv_ds: {}", mul_deriv_ds->as<Lam>()->filter());
            mul_deriv_ds->as_nom<Lam>()->set_filter(true);
            world.DLOG("updated filter of mul_deriv_ds: {}", mul_deriv_ds->as<Lam>()->filter());
            world.DLOG("filter of mul_deriv_ds2: {}", mul_deriv_ds2->as<Lam>()->filter());
            // assert(0);

            return mul_deriv_ds;
        }

        assert(false && "unhandled axiom");
    }

    // TODO: remaining (lambda, axiom)

    world.ELOG("did not expect to augment: {} : {}", def, def->type());
    world.ELOG("node: {}", def->node_name());
    assert(false && "augment not implemented on this def");
}

} // namespace thorin::autodiff