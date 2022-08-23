#include <iostream>

#include <thorin/lam.h>
#include <thorin/tables.h>

#include "dialects/affine/affine.h"
#include "dialects/core/core.h"
// #include "dialects/direct/direct.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

// TODO: maybe use template (https://codereview.stackexchange.com/questions/141961/memoization-via-template) to memoize 
const Def* AutoDiffEval::augment(const Def* def, Lam* f, Lam* f_diff) {
    if (auto i = augmented.find(def); i != augmented.end()) return i->second;
    augmented[def] = augment_(def,f,f_diff);
    return augmented[def];
}

const Def* AutoDiffEval::derive(const Def* def) {
    if (auto i = derived.find(def); i != derived.end()) return i->second;
    derived[def] = derive_(def);
    return derived[def];
}

const Def* id_pullback(const Def* A) {
    auto& world = A->world();
    auto arg_pb_ty = pullback_type(A, A);
    auto id_pb = world.nom_lam(arg_pb_ty, world.dbg("id_pb"));
    auto id_pb_scalar = id_pb->var((nat_t)0, world.dbg("s"));
    // assert(id_pb!=NULL);
    // assert(id_pb->ret_var()!=NULL);
    // assert(id_pb_scalar!=NULL);
    id_pb->app(
        true,
        // id_pb->ret_var(),
        id_pb->var(1),// can not use ret_var as the result might be higher order
        id_pb_scalar
    );

    // world.DLOG("id_pb for type {} is {} : {}",A,id_pb,id_pb->type());
    return id_pb;
}

void AutoDiffEval::create_shadow_id_pb(const Def* def) {
    auto& world = def->world();
    auto A = def->type();

    // TODO: write down a good explanation for
    // ops vs projs vs type ops
    // example: vars
    // ops: fun, ?
    // projs: virtual extracts
    // sigma ops: types of projs

    // base case + id pb for all intermediate
    auto id_pb = id_pullback(A);
    partial_pullback[def] = id_pb;
    world.DLOG("id pb for {} : {} is {} : {}",def,def->type(),id_pb,id_pb->type());

    if(auto sig = A->isa<Sigma>()) {
        //R auto pb_ty= pullback_type(A,A);
        for(auto op:def->projs()) {
            create_shadow_id_pb(op);
        }
        DefArray pbs(def->projs(),
            [&](const Def* op) { return partial_pullback[op]; }
        );
        shadow_pullback[def] = world.tuple(pbs);
        world.DLOG("shadow_pullback[{} : {}] = {} : {}", def, def->type(), shadow_pullback[def], shadow_pullback[def]->type());
        return;
    }
    // TODO: other cases (array, )

    // no structure => needs no structure pullback (base case also needs no str. pb because it is shallow)
}

/// side effect: register pullback
const Def* AutoDiffEval::derive_(const Def* def) {
    auto& world = def->world();
    if(auto lam = def->isa_nom<Lam>()) {
        world.DLOG("Derive lambda: {}", def);
        auto deriv_ty = autodiff_type(lam->type());
        auto deriv = world.nom_lam(deriv_ty, world.dbg(lam->name()+"_deriv"));

        // pre register derivative
        // needed for recursion
        // (could also be used for projecting out the variables
        //  instead of pre-partial-pullback initialization)
        derived[lam] = deriv;

        auto [arg_ty, ret_pi] = lam->type()->doms<2>();
        auto ret_ty = ret_pi->as<Pi>()->dom();

        //R auto arg_ty_tangent = tangent_type(arg_ty);
        // equal to 
        //  deriv->dom(1)->as<Pi>() // return type
        //      ->dom(1)->as<Pi>()  // pb type
        //      ->dom(1)->as<Pi>()->dom() // arg tangent type
        // cn[in_ty, cn[out_ty, cn[out_tan_ty, cn[in_tan_ty]]]]
        //                                        ^~~~~~~~~
        
        // register pullback args
        //R auto arg_pb_ty = world.cn({arg_ty_tangent, world.cn({arg_ty_tangent})});

        // auto arg_pb_ty = pullback_type(arg_ty, arg_ty);
        // auto id_pb = world.nom_lam(arg_pb_ty, world.dbg(lam->name()+"_arg_pb"));
        // auto id_pb_scalar = id_pb->var((nat_t)0, world.dbg("s"));
        // id_pb->app(
        //     true,
        //     id_pb->ret_var(),
        //     id_pb_scalar
        // );

        auto deriv_all_args = deriv->var();
        //R const Def* deriv_arg = deriv->var((nat_t)0, world.dbg("arg"));
        //R auto deriv_ret = deriv->var((nat_t)1, world.dbg("ret"));
        //R partial_pullback[deriv_arg] = id_pb;

        // TODO: shadow pullback for arguments
        // shadow_pullback[deriv_all_args] = build_shadow_id_pb(deriv_all_args->type());
        create_shadow_id_pb(deriv_all_args);



        // TODO: remove as this is subsumed by lam->deriv
        //R const Def* lam_ret   = lam->var(1, world.dbg("ret"));
        //R const Def* deriv_ret = deriv->var(1, world.dbg("ret"));
        //R derived[lam_ret] = deriv_ret;

        // pre-register augment replacements
        // TODO: maybe leave out 
        // function call (duplication with derived)
        augmented[def] = deriv;
        world.DLOG("Associate {} with {}", def, deriv);
        // TODO: transively remove
        // arguments (not necessary)
        // auto src_arg = lam->var()
        augmented[lam->var()] = deriv->var();
        world.DLOG("Associate vars {} with {}", lam->var(), deriv->var());


        // already contains the correct application of 
        // deriv->ret_var() by specification
        // f : cn[R] has a partial derivative (exception to closed rule)
        // f': cn[R, cn[R, cn[A]]]
        //   this is needed for continuations (without closure conversion)
        //   but also essentially for the return continuation

        // reminder of types:
        // expression e: B
        //   implicit: e_fun: A -> B
        // partial pullback: e*: B* -> A*
        // partial derivative: e': B' × (B* -> A*)
        //   implicit: e'_fun: A' -> B' × (B* -> A*)
        auto new_body = augment(lam->body(),lam,deriv);
        deriv->set_filter(true);
        deriv->set_body(new_body);

        return deriv;
    }
}

const Def* zero_pullback(const Def* E, const Def* A) {
    auto& world = A->world();
    auto pb_ty = pullback_type(E,A);
    auto pb = world.nom_lam(pb_ty, world.dbg("zero_pb"));
    world.DLOG("zero_pullback {} -> {}", E, A);
    pb->app(
        true,
        pb->var(1),
        op_zero(A)
    );
    return pb;
}

bool is_closed_function(const Def* e) {
    auto& world = e->world();
    auto E = e->type();
    if(auto pi = E->isa<Pi>()) {
        //R world.DLOG("codom is {}", pi->codom());
        //R world.DLOG("codom kind is {}", pi->codom()->node_name());
        // duck-typing applies here
        // use short-circuit evaluation to reuse previous results
        return
            pi->codom()->isa<Bot>()!=NULL && // continuation
            pi->num_doms()==2 && // args, return
            pi->dom(1)->isa<Pi>()!=NULL; // return type
    }
    return false;
}

/// rewrite the given definition
/// 
const Def* AutoDiffEval::augment_(const Def* def, Lam* f, Lam* f_diff) {
    auto& world = def->world();
    auto f_arg_tangent_ty=tangent_type(f->type()->dom(0));
    // auto f_arg_tangent_ty = 
    //     f_diff->type()->dom(1)->as<Pi>() // return type
    //     ->dom(1)->as<Pi>() // pb type
    //     ->dom(1)->as<Pi>()->dom(); // arg tangent type

    world.DLOG("Augment def {} : {}", def, def->type());

    // app => cont, operator, function
    if(auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto arg = app->arg();
        world.DLOG("Augment application: app {} with {}", callee, arg);

        auto aug_arg = augment(arg,f,f_diff);
        auto aug_callee = augment(callee,f,f_diff);
        auto arg_ppb = partial_pullback[aug_arg];

        if(!is_closed_function(callee)) {
            // TODO: check if function (not operator)
            // original function = unclosed function (return cont / continuation)
            //   Cn[E]
            // aug_calle (looks like a function but is not really)
            //   Cn[E, Cn[E, Cn[A]]]
            
            // ret(e) => ret'(e, e*)

            world.DLOG("continuation {} : {} => {} : {}", callee, callee->type(), aug_callee, aug_callee->type());

            auto arg_pb = partial_pullback[aug_arg];
            auto aug_app = world.app(aug_callee, {aug_arg, arg_pb});
            world.DLOG("Augmented application: {} : {}", aug_app, aug_app->type());
            // assert(false);
            return aug_app;
        }

        // return def;
    }

    // projection
    else if(auto ext = def->isa<Extract>()) {
        // world.DLOG("Augment extract: {}", def);
        auto tuple = ext->tuple();
        auto index = ext->index();
        world.DLOG("Augment extract: {} #[{}]", tuple, index);

        auto aug_tuple = augment(tuple,f,f_diff);
        auto aug_index = augment(index,f,f_diff);

        auto shadow_tuple_pb = shadow_pullback[aug_tuple];

        auto aug_ext = world.extract(aug_tuple, aug_index);
        auto pb = world.extract(shadow_tuple_pb, aug_index);
        partial_pullback[aug_ext] = pb;

        return aug_ext;
    }

    // vars (function argument)
    else if(auto var = def->isa<Var>()) {
        world.DLOG("Augment variable: {}", def);
        assert(augmented.count(def));
        auto aug_var = augmented[def];
        assert(partial_pullback.count(aug_var));
        return var;

        // auto var_ppb = partial_pullbacks[var];
        // if(var_ppb) {
        //     world.DLOG("Augment var: {} with {}", var, var_ppb);
        //     return var_ppb;
        // }
        // return def;

        // TODO: move out to general handling?
    }


    // constants
    else if(auto lit = def->isa<Lit>()) {
        world.DLOG("Augment literal: {}", def);
        auto aug_lit=lit;
        // set zero pullback
        auto pb = zero_pullback(lit->type(), f_arg_tangent_ty);
        partial_pullback[aug_lit] = pb;
        return def;
    }

    // TODO: tuple



    world.ELOG("did not expect to augment: {}", def);
    world.ELOG("node: {}", def->node_name());
    assert(false && "augment only on app");
}

const Def* AutoDiffEval::rewrite(const Def* def) {
    auto& world = def->world();

    if (auto ad_app = match<autodiff>(def); ad_app) {
        // callee = autodiff T
        // arg = function of type T
        //   (or operator)
        // auto callee = ad_app->callee();
        auto arg = ad_app->arg();
        world.DLOG("found a autodiff::autodiff of {}",arg);
        // world.DLOG("found a autodiff::autodiff {} to {}",callee,arg);

        if(arg->isa<Lam>()) {
            // world.DLOG("found a autodiff::autodiff of a lambda");
            return derive(arg);
        }

        assert(0);
        return def;
    }

    return def;
}

PassTag* AutoDiffEval::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::autodiff
