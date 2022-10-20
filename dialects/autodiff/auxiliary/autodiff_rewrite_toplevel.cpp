#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"

#include "dialects/mem/autogen.h"
#include "dialects/affine/autogen.h"
#include "dialects/mem/mem.h"
#include "dialects/affine/affine.h"

namespace thorin::autodiff {

const Def* create_ho_pb_type(const Def* def, const Def* arg_ty){
    if(auto arr = def->isa<Arr>()){
        auto &world = def->world();
        auto shape = arr->shape();
        auto body = create_ho_pb_type(arr->body(), arg_ty);
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

    else if (auto idx = ty->isa<Idx>()) {
        // TODO: real
        auto zero = world.lit_idx(ty, 0, world.dbg("zero"));
        world.DLOG("zero_def for int is {}", zero);
        return zero;
    }
    
    if(match<mem::Ptr>(ty)){
        auto gradient = gradient_ptrs[def];

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

const Def* AutoDiffEval::autodiff_epilogue(Lam* f_outer, Lam* f_inner, const Def* diff_ty){
    auto& world = f_outer->world();

    auto arg = mem::replace_mem(mem::mem_var(f_inner), f_outer->var(0_s));
    if(match<mem::M>(f_outer->dom((nat_t) 0)->proj(0))){
        const Def* current_mem = mem::mem_var(f_outer);

        for(auto var : f_outer->var((nat_t)0)->projs()){
            auto var_ty = var->type();
            if(auto ptr = match<mem::Ptr>(var_ty)){
                auto [mem2, gradient_ptr] = mem::op_alloc(ptr->arg(0), current_mem, world.dbg(var->name() + "_gradient_arr"))->projs<2>();
                current_mem = mem2;

                //auto pb_ty = create_ho_pb_type(ptr->arg(0), diff_ty);
                //auto [mem3, pb_ptr] = mem::op_alloc(pb_ty, mem2, world.dbg(var->name() + "_pullback_arr"))->projs<2>();
                //current_mem = mem3;

                //allocated_memory.insert(pb_ptr);
                gradient_ptrs[var] = gradient_ptr;
                //shadow_pullbacks[var] = pb_ptr;
            }
        }

        f_outer->set_filter(true);
        f_outer->set_body(world.app(f_inner, {current_mem, f_outer->var(1_s)}));
    }

    return world.tuple({arg, f_inner->var(1)});
}

/// side effect: register pullback
const Def* AutoDiffEval::derive_(const Def* def) {
    auto& world = def->world();
    if (auto lam = def->isa_nom<Lam>()) {
        world.DLOG("Derive lambda: {}", def);
        auto deriv_ty = autodiff_type_fun_pi(lam->type());
        auto deriv_outer    = world.nom_lam(deriv_ty, world.dbg(lam->name() + "_deriv_outer"));
        auto memType = mem::type_mem(world);
        auto deriv_inner    = world.nom_lam(world.cn({memType, deriv_outer->ret_pi()}), world.dbg(lam->name() + "_deriv_inner"));


        // pre register derivative
        // needed for recursion
        // (could also be used for projecting out the variables
        //  instead of pre-partial-pullback initialization)
        derived[lam] = deriv_outer;

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

        const Def* var = autodiff_epilogue(deriv_outer, deriv_inner, arg_ty);
        augmented[lam->var()] = var;

        // TODO: check identity
        // could use identity tangent(arg_ty) = tangent(augment(arg_ty))
        // with deriv_arg->type() = augment(arg_ty)
        const Def* deriv_arg = deriv_outer->var((nat_t)0, world.dbg("arg"));
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
        augmented[def] = deriv_outer;
        world.DLOG("Associate {} with {}", def, deriv_outer);
        world.DLOG("  {} : {}", lam, lam->type());
        world.DLOG("  {} : {}", deriv_outer, deriv_outer->type());
        // TODO: transively remove
        // arguments (maybe not necessary)
        // auto src_arg = lam->var()
        
        world.DLOG("Associate vars {} with {}", lam->var(), deriv_outer->var());

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
        auto new_body = augment(lam->body(), lam, deriv_outer);
        deriv_inner->set_filter(true);
        deriv_inner->set_body(new_body);

        return deriv_outer;
    }
}

} // namespace thorin::autodiff