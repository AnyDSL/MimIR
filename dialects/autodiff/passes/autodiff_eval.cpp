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
const Def* AutoDiffEval::augment(const Def* def) {
    if (auto i = augmented.find(def); i != augmented.end()) return i->second;
    augmented[def] = augment_(def);
    return augmented[def];
}

const Def* AutoDiffEval::derive(const Def* def) {
    if (auto i = derived.find(def); i != derived.end()) return i->second;
    derived[def] = derive_(def);
    return derived[def];
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

        auto arg_ty_tangent = tangent_type(arg_ty);
        // equal to 
        //  deriv->dom(1)->as<Pi>() // return type
        //      ->dom(1)->as<Pi>()  // pb type
        //      ->dom(1)->as<Pi>()->dom() // arg tangent type
        // cn[in_ty, cn[out_ty, cn[out_tan_ty, cn[in_tan_ty]]]]
        //                                        ^-------^
        
        // register pullback args
        auto id_pb = world.nom_lam(
            world.cn({arg_ty_tangent, world.cn({arg_ty_tangent})}),
            world.dbg(lam->name()+"_arg_pb"));
        id_pb->app(
            true,
            id_pb->ret_var(),
            id_pb->vars(0)
        );
        const Def* deriv_arg = deriv->var(0, world.dbg("arg"));
        partial_pullbacks[deriv_arg] = id_pb;

        // TODO: remove as this is subsumed by lam->deriv
        // const Def* lam_ret   = lam->var(1, world.dbg("ret"));
        // const Def* deriv_ret = deriv->var(1, world.dbg("ret"));
        // derived[lam_ret] = deriv_ret;

        // pre-register augment replacements
        // TODO: maybe leave out 
        // function call (duplication with derived)
        augmented[def] = deriv;
        // TODO: transively remove
        // arguments (not necessary)
        // auto src_arg = lam->var()
        // augmented[lam->var()] = deriv->var();

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
        auto new_body = augment(lam->body());
        deriv->set_body(new_body);

        return deriv;
    }

}

/// rewrite the given definition
/// 
const Def* AutoDiffEval::augment_(const Def* def) {
    auto& world = def->world();
    if(auto app = def->isa<App>()) {

    }
    world.ELOG("did not expect to augment: {}", def);
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
