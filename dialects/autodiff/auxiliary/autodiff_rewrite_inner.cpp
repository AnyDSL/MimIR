#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/core/core.h"
#include "dialects/direct/direct.h"

namespace thorin::autodiff {

#define f_arg_ty continuation_dom(f->type())

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

const Def* AutoDiffEval::augment_lam(Lam* lam, Lam* f, Lam* f_diff) {
    auto& world = lam->world();
    // TODO: need partial pullbacks for tuples (higher-order / ret-cont application)
    // also for higher-order args, ret_cont (at another point)
    // the pullback is not important but formally required by tuple rule
    if (augmented.count(lam)) {
        // includes lam==f, out == f_diff
        // handle like higher order argument
        // replace with derived function
        world.DLOG("already augmented {} : {} to {} : {}", lam, lam->type(), augmented[lam], augmented[lam]->type());
        return augmented[lam];
    }
    if (is_open_continuation(lam)) {
        // a open continuation is the same as return
        // cont: Cn[X]
        // cont': Cn[X,Cn[X,A]]
        // dependency on closed function context

        world.DLOG("found an open continuation {} : {}", lam, lam->type());
        auto cont_dom = lam->type()->dom(); // not only 0 but all
        auto pb_ty    = pullback_type(cont_dom, f_arg_ty);
        world.DLOG("pb type is {}", pb_ty);
        auto aug_ty = world.cn({cont_dom, pb_ty});
        world.DLOG("augmented type is {}", aug_ty);
        // assert(0);
        auto aug_lam              = world.nom_lam(aug_ty, world.dbg("aug_" + lam->name()));
        auto aug_var              = aug_lam->var((nat_t)0);
        augmented[lam->var()]     = aug_var;
        augmented[lam]            = aug_lam; // TODO: only one of these two
        derived[lam]              = aug_lam;
        auto pb                   = aug_lam->var(1);
        partial_pullback[aug_var] = pb;
        // still in same closed function
        auto new_body = augment(lam->body(), f, f_diff);
        aug_lam->set_filter(lam->filter());
        aug_lam->set_body(new_body);

        // R auto lam_pb_ty = pullback_type(lam->type(), f_arg_ty);
        // R auto lam_pb = world.cn(lam_pb_ty);
        // R lam_pb->app(

        // R )
        auto lam_pb               = zero_pullback(lam->type(), f_arg_ty);
        partial_pullback[aug_lam] = lam_pb;
        world.DLOG("augmented {} : {}", lam, lam->type());
        world.DLOG("to {} : {}", aug_lam, aug_lam->type());
        world.DLOG("ppb for lam cont: {}", lam_pb);

        return aug_lam;
    }
    world.DLOG("found a closed function call {} : {}", lam, lam->type());
    // general function
    auto aug_lam = op_autodiff(lam);
    // TODO: directly more association here?
    world.DLOG("augmented function is {} : {}", aug_lam, aug_lam->type());
    return aug_lam;
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
    world.DLOG("tuple was: {} : {}", tuple, tuple->type());
    world.DLOG("aug tuple: {} : {}", aug_tuple, aug_tuple->type());
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
    DefArray tangents(pbs.size(),
                      [&](nat_t i) { return world.app(direct::op_cps2ds_dep(pbs[i]), world.extract(pb_tangent, i)); });
    pb->app(true, pb->var(1),
            // summed up tangents
            op_sum(tangent_type_fun(f_arg_ty), tangents));
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
        world.debug_dump();
        // assert(false);
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
        assert(0);
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

        // auto X = continuation_codom(g->type());
        // // auto A = f_diff->var((nat_t)0);
        // auto A = f_diff->type()->dom(0);
        // // auto E = g_deriv->var((nat_t)0);
        // auto E = g_deriv->type()->as<Pi>()->dom(0);
        // world.DLOG("A (var f): {}", A);
        // world.DLOG("E (var g): {}", E);
        // world.DLOG("X (out g = out f): {}", X);
        // // auto ret_g_deriv = g_deriv->var(1);
        // auto ret_g_deriv_ty = g_deriv->type()->as<Pi>()->dom(1);
        // world.DLOG("ret_g_deriv_ty: {} ", ret_g_deriv_ty);
        // // auto ret_f_deriv=f_diff->var(1);
        // // world.DLOG("ret_f_deriv: {} : {}", ret_f_deriv, ret_f_deriv->type());

        // // TODO: better debug names
        // auto c1_ty=ret_g_deriv_ty->as<Pi>();
        // world.DLOG("c1_ty: (cn[X, cn[X+, cn E+]]) {}", c1_ty);
        // auto c2_ty=aug_cont->type()->as<Pi>()->dom(2)->as<Pi>();
        // world.DLOG("c2_ty: (cn[X+, cn A+]) {}", c2_ty);
        // auto c3_ty=c1_ty->dom(2)->as<Pi>()->dom(2)->as<Pi>();
        // world.DLOG("c3_ty: (cn E+) {}", c3_ty);
        // auto c1 = world.nom_lam(c1_ty,world.dbg("c1"));
        // auto c2 = world.nom_lam(c2_ty,world.dbg("c2"));
        // auto c3 = world.nom_lam(c3_ty,world.dbg("c3"));

        // c1->app(true,
        //     aug_cont,
        //     {
        //         c1->var((nat_t)0),
        //         c2
        //     }
        // );
        // c2->app(true,
        //     c1->var(1),
        //     {
        //         c2->var((nat_t)0),
        //         c3
        //     }
        // );
        // c3->app(true,
        //     e_pb,
        //     {
        //         c3->var((nat_t)0),
        //         c2->var(1)
        //     }
        // );

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

    // lam
    else if (auto lam = def->isa_nom<Lam>()) {
        world.DLOG("Augment nom lambda: {}", lam);
        return augment_lam(lam, f, f_diff);
    } else if (auto lam = def->isa<Lam>()) {
        world.ELOG("Augment lambda: {}", lam);
        assert(false && "can not handle non-nominal lambdas");
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

            // auto mul_deriv_ds2  = world.lookup("mul_deriv_ds_by_cps");
            // assert(mul_deriv_ds2);

            world.DLOG("filter of mul_deriv_cps: {}", mul_deriv_cps->as<Lam>()->filter());
            mul_deriv_cps->as_nom<Lam>()->set_filter(true);
            world.DLOG("updated filter of mul_deriv_cps: {}", mul_deriv_cps->as<Lam>()->filter());
            return mul_deriv_cps;

            // world.DLOG("filter of mul_deriv_ds: {}", mul_deriv_ds->as<Lam>()->filter());
            // mul_deriv_ds->as_nom<Lam>()->set_filter(true);
            // world.DLOG("updated filter of mul_deriv_ds: {}", mul_deriv_ds->as<Lam>()->filter());
            // // world.DLOG("filter of mul_deriv_ds2: {}", mul_deriv_ds2->as<Lam>()->filter());
            // // assert(0);

            // return mul_deriv_ds;
        }

        assert(false && "unhandled axiom");
    }

    // TODO: remaining (lambda, axiom)

    world.ELOG("did not expect to augment: {} : {}", def, def->type());
    world.ELOG("node: {}", def->node_name());
    assert(false && "augment not implemented on this def");
}

} // namespace thorin::autodiff