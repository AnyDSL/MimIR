#include "dialects/affine/affine.h"
#include "dialects/affine/autogen.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/auxiliary/mem/autodiff_mem_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

void AutoDiffEval::prepareArguments(Lam* lam, Lam* deriv) {
    auto& world = deriv->world();

    auto [arg_ty, ret_pi] = lam->type()->doms<2>();

    auto deriv_all_args  = deriv->var();
    const Def* deriv_arg = deriv->var((nat_t)0, world.dbg("arg"));

    // We generate the shadow pullbacks dynamically to save work and avoid code duplication.
    // Only the toplevel pullback for arguments and return continuation is special cased.

    // TODO: check identity: could use identity tangent(arg_ty) = tangent(augment(arg_ty)) with deriv_arg->type() =
    // augment(arg_ty) We give the argument the identity pullback.
    auto arg_id_pb              = id_pullback(arg_ty);
    partial_pullback[deriv_arg] = arg_id_pb;
    // The return continuation has to formally exist but should never be directly accessed.
    auto ret_var = deriv->var(1);
    // TODO: think about just returning bot instead of zero
    auto ret_pb               = zero_pullback(lam->var(1)->type(), arg_ty);
    partial_pullback[ret_var] = ret_pb;

    shadow_pullback[deriv_all_args] = world.tuple({arg_id_pb, ret_pb});
    world.DLOG("pullback for argument {} : {} is {} : {}", deriv_arg, deriv_arg->type(), arg_id_pb, arg_id_pb->type());
    world.DLOG("args shadow pb is {} : {}", shadow_pullback[deriv_all_args], shadow_pullback[deriv_all_args]->type());

    prepareMemArguments(lam, deriv);
}

/// side effect: register pullback
const Def* AutoDiffEval::derive_(const Def* def) {
    auto& world = def->world();
    auto lam    = def->as_nom<Lam>(); // TODO check if nominal
    world.DLOG("Derive lambda: {}", def);
    auto deriv_ty = autodiff_type_fun_pi(lam->type());
    auto deriv    = world.nom_lam(deriv_ty, world.dbg(lam->name() + "_deriv"));

    // We first pre-register the derivatives.
    // This knowledge is needed for recursion.
    // (Alternatively, we could also use projections out the variables instead of pre-partial-pullback
    // initialization.)
    derived[lam] = deriv;

    // We pre-register the augment replacements.
    // The function and its variables are replaced by their new derived versions.
    // TODO: maybe leave out function call (duplication with derived)
    augmented[def] = deriv;
    world.DLOG("Associate {} with {}", def, deriv);
    world.DLOG("  {} : {}", lam, lam->type());
    world.DLOG("  {} : {}", deriv, deriv->type());
    augmented[lam->var()] = deriv->var();
    world.DLOG("Associate vars {} with {}", lam->var(), deriv->var());

    prepareArguments(lam, deriv);

    // already contains the correct application of
    // deriv->ret_var() by specification
    // f : cn[R] has a partial derivative (exception to closed rule)
    // f': cn[R, cn[R, cn[A]]]
    //   this is needed for continuations (without closure conversion)
    //   but also essentially for the return continuation

    // Here a reminder of types:
    // The expression `e: B` has the implicit function `e_fun: A -> B`
    // The partial pullback is then `e*: B* -> A*`
    // The derivatived version is `e': B' × (B* -> A*)` which is an application of `e'_fun: A' -> B' × (B* -> A*)`
    auto new_body = augment(lam->body(), lam, deriv);
    deriv->set_filter(lam->filter());
    deriv->set_body(new_body);

    return deriv;
}

} // namespace thorin::autodiff
