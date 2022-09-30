#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"

namespace thorin::autodiff {

/// side effect: register pullback
const Def* AutoDiffEval::derive_(const Def* def) {
    auto& world = def->world();
    if (auto lam = def->isa_nom<Lam>()) {
        world.DLOG("Derive lambda: {}", def);
        auto deriv_ty = autodiff_type_fun_pi(lam->type());
        auto deriv    = world.nom_lam(deriv_ty, world.dbg(lam->name() + "_deriv"));

        // pre register derivative
        // needed for recursion
        // (could also be used for projecting out the variables
        //  instead of pre-partial-pullback initialization)
        derived[lam] = deriv;

        auto [arg_ty, ret_pi] = lam->type()->doms<2>();
        auto ret_ty           = ret_pi->as<Pi>()->dom();

        auto deriv_all_args  = deriv->var();
        const Def* deriv_arg = deriv->var((nat_t)0, world.dbg("arg"));

        // let shadow pb be created dynamically
        // only handle toplevel [args, ret] specially with a pseudo shadow pb
        //
        // but the DS/CPS special case has to be handled separately

        // TODO: check identity
        // could use identity tangent(arg_ty) = tangent(augment(arg_ty))
        // with deriv_arg->type() = augment(arg_ty)
        auto arg_id_pb              = id_pullback(arg_ty);
        partial_pullback[deriv_arg] = arg_id_pb;
        // set no pullback to all_arg and return
        // second component has to exist but should not be accessed
        auto ret_var              = deriv->var(1);
        auto ret_pb               = zero_pullback(lam->var(1)->type(), arg_ty);
        partial_pullback[ret_var] = ret_pb;

        shadow_pullback[deriv_all_args] = world.tuple({arg_id_pb, ret_pb});
        world.DLOG("pullback for argument {} : {} is {} : {}", deriv_arg, deriv_arg->type(), arg_id_pb,
                   arg_id_pb->type());
        world.DLOG("args shadow pb is {} : {}", shadow_pullback[deriv_all_args],
                   shadow_pullback[deriv_all_args]->type());

        // pre-register augment replacements
        // TODO: maybe leave out
        // function call (duplication with derived)
        augmented[def] = deriv;
        world.DLOG("Associate {} with {}", def, deriv);
        world.DLOG("  {} : {}", lam, lam->type());
        world.DLOG("  {} : {}", deriv, deriv->type());
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
        auto new_body = augment(lam->body(), lam, deriv);
        deriv->set_filter(true);
        deriv->set_body(new_body);

        return deriv;
    }
}

} // namespace thorin::autodiff