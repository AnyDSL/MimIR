#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/auxiliary/mem/autodiff_mem_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

// TODO remove macro
#define f_arg_ty continuation_dom(f->type())

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

    // TODO: go deeper

    // for (auto arg : deriv_arg->projs()) {
    //     auto arg_ty = arg->type();
    //     if (auto ptr = match<mem::Ptr>(arg_ty)) {
    //         auto [ptr_ty, addr_space] = ptr->args<2>();
    //         auto [mem2, gradient_ptr] =
    //             mem::op_alloc(ptr_ty, current_mem, world.dbg(arg->name() + "_gradient_arr"))->projs<2>();
    //         current_mem = mem2;
    //         gradient_ptrs[arg] = gradient_ptr;
    //     }
    // }

    // Reassociate the arguments to replace the old memory with the new one.
    // We reassociate all arguments together to prevent early skips.
    // TODO: test if this works as intended
    // deriv_mem |-> current_mem
    // Alternatively to replace_mem, a subst call could be used.
    augmented[lam->var()] = mem::replace_mem(current_mem, deriv->var());
}

const Def* AutoDiffEval::wrap_call_pullbacks(const Def* arg_pb, const Def* arg) {
    auto& w = arg->world();

    DefVec pullbacks;
    for (auto arg_proj : arg->projs()) {
        auto augment_arg_proj = augmented[arg_proj];
        if (!augment_arg_proj) continue;
        auto pullback_root = shadow_pullback[augment_arg_proj];
        if (!pullback_root) continue;
        pullbacks.push_back(pullback_root);
    }

    auto propagate_gradients = w.nom_lam(arg_pb->type()->as<Pi>(), w.dbg("propagate_gradients"));
    propagate_gradients->set_filter(false);

    DefVec gradients;
    for (auto var : propagate_gradients->var(0_s)->projs()) {
        if (match<mem::Ptr>(var->type())) { gradients.push_back(var); }
    }

    auto pullbacks_size = pullbacks.size();
    auto gradients_size = gradients.size();

    assert(pullbacks_size == gradients_size);

    auto current = propagate_gradients;

    for (size_t i = 0; i < pullbacks_size; i++) {
        auto gradient = gradients[i];
        auto pullback = pullbacks[i];

        auto next     = mem_lam(w, "next_loop", false);
        auto loop_lam = call_pullbacks(gradient, pullback);
        current->set_body(w.app(loop_lam, {mem::mem_var(current), next}));
        current = next;
    }

    auto exit_arg = mem::replace_mem(mem::mem_var(current), propagate_gradients->var());
    current->set_body(w.app(arg_pb, exit_arg));
    return propagate_gradients;
}

Lam* AutoDiffEval::free_memory_lam() {
    auto& w = world();

    auto free = mem_return_lam(w, "free");

    auto mem = mem::mem_var(free);
    // TODO: handle via new alloc axiom variant
    for (auto memory : allocated_memory) { mem = mem::op_free(mem, memory); }

    free->set_body(w.app(free->var(1_s), mem));
    return free;
}

// TODO: rename to make connection to load clear
Lam* AutoDiffEval::create_gradient_collector(const Def* gradient_lea, Lam* f) {
    // load  : Mem * Ptr -> Mem * T
    // load' : cn[[Mem^T, T^T], cn[Mem^T, Ptr^T]]
    //            pb_mem, pb_s

    auto& w               = world();
    auto elem_ty          = match<mem::Ptr>(gradient_lea->type())->arg(0);
    auto [arg_ty, ret_pi] = f->type()->doms<2>();
    auto pb_type          = pullback_type(elem_ty, arg_ty);

    auto pb_lam = w.nom_lam(pb_type, w.dbg("load_pb"));

    auto [pb_arg, pb_ret] = pb_lam->vars<2>();
    auto [pb_mem, pb_s]   = pb_arg->projs<2>();

    // TODO: can we generalize add even more? (maybe even on ptr?)
    auto [gradient_mem, gradient] = mem::op_load(pb_mem, gradient_lea, w.dbg("gradient_array_load"))->projs<2>();
    auto add                      = op_add(gradient, pb_s);
    auto store_mem                = mem::op_store(gradient_mem, gradient_lea, add, w.dbg("add_to_gradient"));

    // TODO: before the gradient was returned; is this necessary?
    // TODO: we want to return the gradient ptrs here (and zero for others)
    auto ptr_zero = op_zero(pb_ret->type()->as<Pi>()->dom(1));

    pb_lam->set_body(w.app(pb_ret, {store_mem, ptr_zero}));
    pb_lam->set_filter(true);

    return pb_lam;
}

const Def* AutoDiffEval::zero_pullback_fun(const Def* domain, Lam* f) {
    const Def* A   = f_arg_ty;
    auto& world    = A->world();
    auto A_tangent = tangent_type_fun(A);
    auto pb_ty     = pullback_type(domain, A);
    auto pb        = world.nom_lam(pb_ty, world.dbg("zero_pb"));
    // TODO: use lazy zero to delay execution as long as possible and allow for shortcut evaluation
    // pb->app(true, pb->var(1), autodiff_zero(mem::mem_var(pb), f));
    pb->app(true, pb->var(1), op_zero(A_tangent));
    return pb;
}

} // namespace thorin::autodiff
