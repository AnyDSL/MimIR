#include "dialects/autodiff/auxiliary/autodiff_mem.h"

#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

const Def* shadow_array_type(const Def* def, const Def* arg_ty) {
    if (auto arr = def->isa<Arr>()) {
        auto& world = def->world();
        auto shape  = arr->shape();
        // TODO: does this need to be a deep structure?
        auto body = shadow_array_type(arr->body(), arg_ty);
        return world.arr(shape, body);
    }

    auto pb_ty = pullback_type(def, arg_ty);
    return pb_ty;
}

// TODO: remove
const Def* AutoDiffEval::autodiff_zero(const Def* mem, Lam* f) {
    auto mapped = augmented[f->var()];

    return autodiff_zero(mem, mapped->proj(0));
}

// TODO: remove (and incorporate two special cases to other)
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

    // def->dump();
    // def->type()->dump();
    assert(false && "unhandled type in autodiff_zero");
}

void AutoDiffEval::prepareMemArguments(Lam* lam, Lam* deriv) {
    const Def* deriv_mem = mem::mem_var(deriv);
    if (!deriv_mem) return;
    const Def* current_mem = deriv_mem;

    auto& world          = deriv->world();
    const Def* deriv_arg = deriv->var((nat_t)0, world.dbg("arg"));

    for (auto arg : deriv_arg->projs()) {
        auto arg_ty = arg->type();
        if (auto ptr = match<mem::Ptr>(arg_ty)) {
            auto [mem2, gradient_ptr] =
                mem::op_alloc(ptr->arg(0), current_mem, world.dbg(arg->name() + "_gradient_arr"))->projs<2>();
            current_mem = mem2;

            gradient_ptrs[arg] = gradient_ptr;
        }
    }

    // Reassociate the arguments to replace the old memory with the new one.
    // We reassociate all arguments together to prevent early skips.
    // TODO: test if this works as intended
    // deriv_mem |-> current_mem
    // Alternatively to replace_mem, a subst call could be used.
    augmented[lam->var()] = mem::replace_mem(current_mem, deriv->var());
}

} // namespace thorin::autodiff
