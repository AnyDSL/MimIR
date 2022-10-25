#include "dialects/affine/affine.h"
#include "dialects/affine/autogen.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

const Def* create_ho_pb_type(const Def* def, const Def* arg_ty) {
    if (auto arr = def->isa<Arr>()) {
        auto& world = def->world();
        auto shape  = arr->shape();
        auto body   = create_ho_pb_type(arr->body(), arg_ty);
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

    if (match<mem::M>(ty)) {
        return mem;
    }

    else if (Idx::size(ty)) {
        // TODO: real
        auto zero = world.lit(ty, 0, world.dbg("zero"));
        world.DLOG("zero_def for int is {}", zero);
        return zero;
    }

    if (match<mem::Ptr>(ty)) {
        auto gradient = gradient_ptrs[def];

        if (gradient == nullptr) { return world.top(ty); }

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

const Def* AutoDiffEval::autodiff_epilogue(Lam* f_outer, Lam* f_inner, const Def* diff_ty) {
    auto& world = f_outer->world();

    auto arg = mem::replace_mem(mem::mem_var(f_inner), f_outer->var(0_s));
    if (match<mem::M>(f_outer->dom((nat_t)0)->proj(0))) {
        const Def* current_mem = mem::mem_var(f_outer);

        for (auto var : f_outer->var((nat_t)0)->projs()) {
            auto var_ty = var->type();
            if (auto ptr = match<mem::Ptr>(var_ty)) {
                auto [mem2, gradient_ptr] =
                    mem::op_alloc(ptr->arg(0), current_mem, world.dbg(var->name() + "_gradient_arr"))->projs<2>();
                current_mem = mem2;

                // auto pb_ty = create_ho_pb_type(ptr->arg(0), diff_ty);
                // auto [mem3, pb_ptr] = mem::op_alloc(pb_ty, mem2, world.dbg(var->name() +
                // "_pullback_arr"))->projs<2>(); current_mem = mem3;

                // allocated_memory.insert(pb_ptr);
                gradient_ptrs[var] = gradient_ptr;
                // shadow_pullbacks[var] = pb_ptr;
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
    // TODO: assert nominal
    if (auto lam = def->isa_nom<Lam>()) {
        world.DLOG("Derive lambda: {}", def);
        auto deriv_ty = autodiff_type_fun_pi(lam->type());
        // TODO: remove redundant function
        auto deriv_outer = world.nom_lam(deriv_ty, world.dbg(lam->name() + "_deriv_outer"));
        auto memType     = mem::type_mem(world);
        auto deriv_inner =
            world.nom_lam(world.cn({memType, deriv_outer->ret_pi()}), world.dbg(lam->name() + "_deriv_inner"));

        // We first pre-register the derivatives.
        // This knowledge is needed for recursion.
        // (Alternatively, we could also use projections out the variables instead of pre-partial-pullback
        // initialization.)
        derived[lam] = deriv_outer;

        auto [arg_ty, ret_pi] = lam->type()->doms<2>();
        auto deriv_all_args   = deriv_outer->var();
        // const Def* deriv_arg  = deriv->var((nat_t)0, world.dbg("arg"));

        const Def* deriv_arg = deriv_outer->var((nat_t)0, world.dbg("arg"));

        // We generate the shadow pullbacks dynamically to save work and avoid code duplication.
        // Only the toplevel pullback for arguments and return continuation is special cased.

        // TODO: check identity: could use identity tangent(arg_ty) = tangent(augment(arg_ty)) with deriv_arg->type() =
        // augment(arg_ty) We give the argument the identity pullback.
        auto arg_id_pb              = id_pullback(arg_ty);
        partial_pullback[deriv_arg] = arg_id_pb;
        // The return continuation has to formally exist but should never be directly accessed.
        auto ret_var = deriv_inner->var(1);
        // auto ret_pb               = zero_pullback(lam->var(1)->type(), arg_ty);
        auto ret_pb               = zero_pullback(lam->var(1)->type(), lam);
        partial_pullback[ret_var] = ret_pb;

        shadow_pullback[deriv_all_args] = world.tuple({arg_id_pb, ret_pb});
        world.DLOG("pullback for argument {} : {} is {} : {}", deriv_arg, deriv_arg->type(), arg_id_pb,
                   arg_id_pb->type());
        world.DLOG("args shadow pb is {} : {}", shadow_pullback[deriv_all_args],
                   shadow_pullback[deriv_all_args]->type());

        // We pre-register the augment replacements.
        // The function and its variables are replaced by their new derived versions.
        // TODO: maybe leave out function call (duplication with derived)
        augmented[def] = deriv_outer;
        world.DLOG("Associate {} with {}", def, deriv_outer);
        world.DLOG("  {} : {}", lam, lam->type());
        world.DLOG("  {} : {}", deriv_outer, deriv_outer->type());
        // augmented[lam->var()] = deriv->var();

        const Def* var        = autodiff_epilogue(deriv_outer, deriv_inner, arg_ty);
        augmented[lam->var()] = var;
        world.DLOG("Associate vars {} with {}", lam->var(), deriv_outer->var());

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
        auto new_body = augment(lam->body(), lam, deriv_outer);
        deriv_inner->set_filter(true);
        deriv_inner->set_body(new_body);
        return deriv_outer;
    }
    // TODO: handle else
}

} // namespace thorin::autodiff
