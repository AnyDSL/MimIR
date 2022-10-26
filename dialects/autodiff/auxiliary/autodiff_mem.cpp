#include "dialects/autodiff/auxiliary/autodiff_mem.h"

#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
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

} // namespace thorin::autodiff
