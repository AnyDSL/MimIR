#include "dialects/affine/affine.h"
#include "dialects/affine/autogen.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/builder.h"
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

const Def* AutoDiffEval::autodiff_zero(const Def* mem) {
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

    else if (auto idx = ty->isa<Idx>()) {
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
    world.bot(def);
}

void AutoDiffEval::fetch_gradients(Lam* diffee, Lam* backward) {
    auto forward_arg  = diffee->var(0_s);
    auto backward_arg = backward->var(0_s);

    size_t fwd_i = 0;
    size_t bwd_i = 0;
    for (auto def : forward_arg->projs()) {
        if (match<mem::Ptr>(def->type())) {
            auto variable = forward_arg->proj(fwd_i);
            auto gradient = backward_arg->proj(bwd_i);

            gradient_ptrs[variable] = gradient;
            bwd_i++;
        } else if (match<mem::M>(def->type())) {
            bwd_i++;
        }

        fwd_i++;
    }
}

const Def* AutoDiffEval::build_gradient_tuple(const Def* mem, Lam* diffee) {
    auto& w         = world();
    auto diffee_arg = diffee->var(0_s);

    DefVec ops;
    for (auto def : diffee_arg->projs()) {
        if (match<mem::M>(def->type())) {
            ops.push_back(mem);
            continue;
        } else if (match<mem::Ptr>(def->type())) {
            auto gradient_ptr = gradient_ptrs[def];
            assert(gradient_ptr);
            ops.push_back(gradient_ptr);
        } else if (def->type()->isa<Idx>()) {
            ops.push_back(zero(w));
        }
    }

    return w.tuple(ops);
}

Lam* AutoDiffEval::init_sinks() {
    auto& w = world();

    auto sinks = create_block("gradient_sinks");

    for (auto load : loads_) {
        auto type = load->proj(1)->type();
        auto slot = op_slot(type, "sink");
        op_store(slot, zero(w));
        gradient_sinks[load] = slot;
    }

    return sinks;
}

Lam* AutoDiffEval::free_memory() {
    auto& w = world();

    auto free_memory = create_block("free_memory");

    for (auto ptr : allocated_memory) { op_free(ptr); }

    ret(free_memory);
    return free_memory;
}

/// side effect: register pullback
const Def* AutoDiffEval::derive_(const Def* def) {
    auto& w     = world();
    auto diffee = def->isa_nom<Lam>();
    assert(diffee);

    auto diff_ty = autodiff_type_fun_pi(diffee->type());

    auto diff_lam = w.nom_lam(diff_ty, w.dbg("diff_forward_" + diffee->name()));
    diff_lam->set_filter(true);

    auto inv_lam_ty = forward_to_backward(diffee->type()->as<Pi>());
    auto inv_lam    = w.nom_lam(inv_lam_ty, w.dbg("diff_backward_" + diffee->name()));
    inv_lam->set_filter(true);

    auto lam_return_ty = diffee->type()->as<Pi>()->dom(1)->as<Pi>();
    auto forward_end   = w.nom_lam(lam_return_ty, w.dbg("forward_end"));
    forward_end->set_filter(true);

    augmented[diffee->var()]    = diff_lam->var();
    augmented[diffee->var(1_s)] = forward_end;

    fetch_gradients(diffee, inv_lam);

    current_state = State::Augment;
    auto aug_body = augment(diffee->body());
    diff_lam->set_body(aug_body);

    auto init_sink_lam = init_sinks();
    auto init_sink_mem = end_mem();

    auto arg        = diffee->var(0_s);
    auto return_var = diffee->var(diffee->num_vars() - 1);
    add_inverted(return_var, create_invert_end());
    add_inverted(arg, diff_lam->var(0_s));
    add_inverted(mem::mem_def(arg), create_invert_end());

    current_state          = State::Invert;
    auto raw_backward_pass = invert(diffee->body());

    auto free_memory_lam = free_memory();

    forward_end->set_body(w.app(diff_lam->var(1_s), {forward_end->var(), inv_lam}));
    auto backward_pass = chain(init_sink_mem, init_sink_lam, raw_backward_pass);

    auto backward_end    = create_return("backward");
    auto gradient_result = build_gradient_tuple(end_mem(), diffee);
    backward_end->set_body(w.app(inv_lam->var(1_s), gradient_result));
    inv_lam->set_body(w.app(backward_pass, {mem::mem_var(inv_lam), backward_end}));

    return diff_lam;
}

} // namespace thorin::autodiff