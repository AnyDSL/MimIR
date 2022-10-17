#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"

#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

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

const Def* hoa(const Def* def, const Def* arg_ty){
    if(auto arr = def->isa<Arr>()){
        auto &world = def->world();
        auto shape = arr->shape();
        auto body = hoa(arr->body(), arg_ty);
        return world.arr(shape, body);
    }

    auto pb_ty = pullback_type(def, arg_ty);
    return pb_ty;
}

const Def* AutoDiffEval::autodiff_zero(const Def* mem, Lam* f) {
    auto mapped = augmented[f->var()];

    return autodiff_zero(mem, mapped->proj(0));
}

const Def* AutoDiffEval::autodiff_zero(const Def* mem, const Def* def) {
    auto& world = def->world();

    auto ty = def->type();

    if (auto tup = def->isa<Tuple>()) {
        DefArray ops(tup->ops(), [&](const Def* op) { return autodiff_zero(mem, op); });
        return world.tuple(ops);
    }
    
    if(match<mem::M>(ty)){
        return mem;
    }

    if (auto app = ty->isa<App>()) {
        auto callee = app->callee();
        // auto args = app->args();
        world.DLOG("app callee: {} : {} <{}>", callee, callee->type(), callee->node_name());
        // TODO: can you directly match Tag::Int?
        if (callee->isa<Idx>()) {
            // auto size = app->arg(0);
            auto zero = world.lit_idx(ty, 0, world.dbg("zero"));
            // world.DLOG("zero_def for int of size {} is {}", size, zero);
            world.DLOG("zero_def for int is {}", zero);
            return zero;
        }
    } 

     else if (auto idx = ty->isa<Idx>()) {
        // TODO: real
        auto zero = world.lit_idx(ty, 0, world.dbg("zero"));
        world.DLOG("zero_def for int is {}", zero);
        return zero;
    }
    
    if(match<mem::Ptr>(ty)){
        auto gradient = shadow_gradient_array[def];

        if(gradient == nullptr){
            return world.top(ty);
        }

        return gradient;
    }

    if (def->type()->isa<Sigma>()) {
        DefArray ops(def->projs(), [&](const Def* op) { return autodiff_zero(mem, op); });
        return world.tuple(ops);
    }

    def->dump();
    def->type()->dump();
    assert(false);
}

/// side effect: register pullback
const Def* AutoDiffEval::derive_(const Def* def) {
    auto& world = def->world();
    if (auto lam = def->isa_nom<Lam>()) {
        world.DLOG("Derive lambda: {}", def);
        auto deriv_ty = autodiff_type_fun_pi(lam->type());
        auto deriv    = world.nom_lam(deriv_ty, world.dbg(lam->name() + "_deriv"));
        auto memType = mem::type_mem(world);
        auto deriv_inner    = world.nom_lam(world.cn({memType, deriv->ret_pi()}), world.dbg(lam->name() + "_deriv_inner"));


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

        // R auto deriv_ret = deriv->var((nat_t)1, world.dbg("ret"));
        // R partial_pullback[deriv_arg] = id_pb;

        // R shadow pullback for arguments
        // R shadow_pullback[deriv_all_args] = build_shadow_id_pb(deriv_all_args->type());
        //  create_shadow_id_pb(deriv_all_args);

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

        const Def* var = deriv->var();

        auto vars = var->projs();
        if(match<mem::M>(deriv->dom((nat_t) 0)->proj(0))){
            auto arg = vars[0];
            current_mem = arg->proj(0);
            auto args = arg->projs();
            args[0] = deriv_inner->var(0_s);
            arg = world.tuple(args);

            for(auto var : deriv->var((nat_t)0)->projs()){
                auto var_ty = var->type();
                if(auto ptr = match<mem::Ptr>(var_ty)){

                    auto [mem2, gradient_ptr] = mem::op_malloc(ptr->arg(0), current_mem, world.dbg("gradient_arr"))->projs<2>();
                    auto pb_ty = hoa(ptr->arg(0), arg_ty);
                    auto [mem3, pb_ptr] = mem::op_malloc(pb_ty, mem2, world.dbg("pullback_arr"))->projs<2>();
                    current_mem = mem3;

                    shadow_gradient_array[var] = gradient_ptr;
                    shadow_pullback_array[var] = pb_ptr;
                    var_ty->dump();
                }
            }
            
            vars[0] = arg;
        }

        vars[1] = deriv_inner->var(1);
        var = world.tuple(vars);

        augmented[lam->var()] = var;

        //auto deriv_all_args  = deriv->var();

        // TODO: check identity
        // could use identity tangent(arg_ty) = tangent(augment(arg_ty))
        // with deriv_arg->type() = augment(arg_ty)
        const Def* deriv_arg = deriv->var((nat_t)0, world.dbg("arg"));
        auto arg_id_pb              = id_pullback(arg_ty);
        partial_pullback[deriv_arg] = arg_id_pb;
        // set no pullback to all_arg and return
        // second component has to exist but should not be accessed
        auto ret_var = deriv_inner->var(1);
        // auto ret_pb=world.bot(world.type_bot());
        // auto ret_pb = zero_pullback(ret_var->type(), arg_ty);
        auto ret_pb               = zero_pullback(lam->var(1)->type(), lam);
        partial_pullback[ret_var] = ret_pb;

        shadow_pullback[var] = world.tuple({arg_id_pb, ret_pb});
        world.DLOG("pullback for argument {} : {} is {} : {}", deriv_arg, deriv_arg->type(), arg_id_pb,
                   arg_id_pb->type());
        world.DLOG("args shadow pb is {} : {}", shadow_pullback[var],
                   shadow_pullback[var]->type());

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
        deriv_inner->set_filter(true);
        deriv_inner->set_body(new_body);

       /* auto result_lam = world.nom_lam(deriv_inner->dom(1)->as<Pi>(), world.dbg("stub"));
        auto result = result_lam->var(0_s);
        auto result_pullback = result_lam->var(1_s);
        result_lam->set_body();

        
        auto test2 = world.nom_lam(test->dom(1)->as<Pi>(), world.dbg("stub"));
        auto test3 = world.nom_lam(test2->dom(1)->as<Pi>(), world.dbg("stub"));

        auto ret_var = deriv->proj(1);
        */

        deriv->set_filter(true);
        deriv->set_body(world.app(deriv_inner, {current_mem, deriv->var(1_s)}));
        deriv->dump(10);

        return deriv;
    }
}

} // namespace thorin::autodiff