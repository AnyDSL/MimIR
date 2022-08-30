#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"

namespace thorin::autodiff {

// void AutoDiffEval::create_shadow_id_pb(const Def* def) {
//     auto& world = def->world();
//     auto A      = def->type();

//     // specialized to main argument


// //     // TODO: write down a good explanation for
// //     // ops vs projs vs type ops
// //     // example: vars
// //     // ops: fun, ?
// //     // projs: virtual extracts
// //     // sigma ops: types of projs

// //     // base case + id pb for all intermediate
// //     auto id_pb            = id_pullback(A);
// //     partial_pullback[def] = id_pb;
// //     world.DLOG("id pb for {} : {} is {} : {}", def, def->type(), id_pb, id_pb->type());

// //     if (auto sig = A->isa<Sigma>()) {
// //         // R auto pb_ty= pullback_type(A,A);
// //         for (auto op : def->projs()) { create_shadow_id_pb(op); }
// //         DefArray pbs(def->projs(), [&](const Def* op) { return partial_pullback[op]; });
// //         shadow_pullback[def] = world.tuple(pbs);
// //         world.DLOG("shadow_pullback[{} : {}] = {} : {}", def, def->type(), shadow_pullback[def],
// //                    shadow_pullback[def]->type());
// //         return;
// //     }else if(auto arr = A->isa<Arr>()) {
// //         // auto body = arr->

// //     }else{
// //         world.DLOG("base case shadow pb {} <{}> : {} <{}>", def, def->node_name(), A, A->node_name());
// //     }
// //     // TODO: other cases (array, )

// //     // no structure => needs no structure pullback (base case also needs no str. pb because it is shallow)
// }

/// side effect: register pullback
const Def* AutoDiffEval::derive_(const Def* def) {
    auto& world = def->world();
    if (auto lam = def->isa_nom<Lam>()) {
        world.DLOG("Derive lambda: {}", def);
        auto deriv_ty = autodiff_type_fun(lam->type());
        auto deriv    = world.nom_lam(deriv_ty, world.dbg(lam->name() + "_deriv"));

        // pre register derivative
        // needed for recursion
        // (could also be used for projecting out the variables
        //  instead of pre-partial-pullback initialization)
        derived[lam] = deriv;

        auto [arg_ty, ret_pi] = lam->type()->doms<2>();
        auto ret_ty           = ret_pi->as<Pi>()->dom();

        // R auto arg_ty_tangent = tangent_type_fun(arg_ty);
        //  equal to
        //   deriv->dom(1)->as<Pi>() // return type
        //       ->dom(1)->as<Pi>()  // pb type
        //       ->dom(1)->as<Pi>()->dom() // arg tangent type
        //  cn[in_ty, cn[out_ty, cn[out_tan_ty, cn[in_tan_ty]]]]
        //                                         ^~~~~~~~~

        // register pullback args
        // R auto arg_pb_ty = world.cn({arg_ty_tangent, world.cn({arg_ty_tangent})});

        // auto arg_pb_ty = pullback_type(arg_ty, arg_ty);
        // auto id_pb = world.nom_lam(arg_pb_ty, world.dbg(lam->name()+"_arg_pb"));
        // auto id_pb_scalar = id_pb->var((nat_t)0, world.dbg("s"));
        // id_pb->app(
        //     true,
        //     id_pb->ret_var(),
        //     id_pb_scalar
        // );

        auto deriv_all_args = deriv->var();
        const Def* deriv_arg = deriv->var((nat_t)0, world.dbg("arg"));
        // R auto deriv_ret = deriv->var((nat_t)1, world.dbg("ret"));
        // R partial_pullback[deriv_arg] = id_pb;

        //R shadow pullback for arguments
        //R shadow_pullback[deriv_all_args] = build_shadow_id_pb(deriv_all_args->type());
        // create_shadow_id_pb(deriv_all_args);

        // let shadow pb be created dynamically
        // only handle toplevel [args, ret] specially with a pseudo shadow pb
        // TODO: move to header
        // short theory of shadow pb:
        // t: [B0, ..., Bn]
        // t*: [B0, ..., Bn] -> A
        // t*_S: [B0 -> A, ..., Bn -> A]
        // b = t#i : Bi
        // b* : Bi -> A
        // b* = t*_S #i (if exists)
        // equivalent to 
        //    \lambda (s:Bi). t*_S (insert s at i in (zero [B0, ..., Bn]))
        // 
        // but the DS/CPS special case has to be handled separately

        // TODO: check identity
        // could use identity tangent(arg_ty) = tangent(augment(arg_ty))
        // with deriv_arg->type() = augment(arg_ty)
        auto arg_id_pb = id_pullback(arg_ty);
        partial_pullback[deriv_arg] = arg_id_pb;
        // set no pullback to all_arg and return
        // second component has to exist but should not be accessed
        auto ret_var = deriv->var(1);
        // auto ret_pb=world.bot(world.type_bot());
        // auto ret_pb = zero_pullback(ret_var->type(), arg_ty);
        auto ret_pb = zero_pullback(lam->var(1)->type(), arg_ty);
        partial_pullback[ret_var] = ret_pb;

        shadow_pullback[deriv_all_args] = world.tuple({arg_id_pb,ret_pb});
        world.DLOG("pullback for argument {} : {} is {} : {}", deriv_arg, deriv_arg->type(), arg_id_pb, arg_id_pb->type());
        world.DLOG("args shadow pb is {} : {}", shadow_pullback[deriv_all_args], shadow_pullback[deriv_all_args]->type());


        // TODO: remove as this is subsumed by lam->deriv
        // R const Def* lam_ret   = lam->var(1, world.dbg("ret"));
        // R const Def* deriv_ret = deriv->var(1, world.dbg("ret"));
        // R derived[lam_ret] = deriv_ret;

        // pre-register augment replacements
        // TODO: maybe leave out
        // function call (duplication with derived)
        augmented[def] = deriv;
        world.DLOG("Associate {} with {}", def, deriv);
        world.DLOG("  {} : {}", lam, lam->type());
        world.DLOG("  {} : {}", deriv, deriv->type());
        // TODO: transively remove
        // arguments (maybe not necessary)
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
        auto new_body = augment(lam->body(), lam, deriv);
        deriv->set_filter(true);
        deriv->set_body(new_body);

        return deriv;
    }
}

} // namespace thorin::autodiff