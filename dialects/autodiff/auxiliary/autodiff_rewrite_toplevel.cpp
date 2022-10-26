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

    assert(false);
    world.bot(def);
}

void AutoDiffEval::fetch_gradients(Lam* src, Lam* backward) {
    auto src_arg  = src->arg();
    auto backward_arg = backward->arg();

    size_t fwd_i = 0;
    size_t bwd_i = 0;
    for (auto def : src_arg->projs()) {
        if (match<mem::Ptr>(def->type())) {
            auto variable = src_arg->proj(fwd_i);
            auto gradient = backward_arg->proj(bwd_i);

            gradient_ptrs[variable] = gradient;
            bwd_i++;
        } else if (match<mem::M>(def->type())) {
            bwd_i++;
        }

        fwd_i++;
    }
}

const Def* AutoDiffEval::build_result(const Def* mem, Lam* backward_begin, Lam* backward_result) {
    auto& w         = world();
    auto diffee_arg = backward_result->var();

    DefVec ops;
    for (auto def : diffee_arg->projs()) {
        if (match<mem::M>(def->type())) {
            ops.push_back(mem);
            continue;
        } else if (match<mem::Ptr>(def->type())) {
            auto gradient_ptr = gradient_ptrs[def];
            assert(gradient_ptr);
            ops.push_back(gradient_ptr);
        } else if (auto app = def->type()->isa<App>()) {
            if (app->callee()->isa<Idx>()) {
                ops.push_back(zero(w));

            } else {
                assert(false);
            }
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

const Def* mask(const Def* target, size_t i, const Def* def) {
    auto& w    = target->world();
    auto projs = target->projs();
    projs[i]   = def;
    return w.tuple(projs);
}

const Def* mask_first(const Def* target, const Def* def) { return mask(target, 0, def); }

const Def* mask_last(const Def* target, const Def* def) { return mask(target, target->num_projs() - 1, def); }

const Def* merge_flat(const Def* left, const Def* right){
    auto& w    = left->world();
    return flatten_deep(w.tuple({left, right}));
}

/// side effect: register pullback
const Def* AutoDiffEval::derive_(const Def* def) {
    auto& w     = world();
    auto diffee = def->isa_nom<Lam>();
    assert(diffee);

    auto diff_ty = autodiff_type_fun_pi(diffee->type());

    auto diff_lam = w.nom_lam(diff_ty, w.dbg("forward_begin_" + diffee->name()));
    diff_lam->set_filter(true);

    auto inv_lam_ty = forward_to_backward(diffee->type()->as<Pi>());
    auto backward_begin    = w.nom_lam(inv_lam_ty, w.dbg("backward_begin_" + diffee->name()));
    backward_begin->set_filter(true);

    auto lam_return_ty = diffee->type()->ret_pi();
    auto forward_end   = w.nom_lam(lam_return_ty, w.dbg("forward_end_" + diffee->name()));
    forward_end->set_filter(true);

    augmented[diffee->var()] = mask_last(diff_lam->var(), forward_end);

    fetch_gradients(diffee, backward_begin);

    current_state = State::Augment;
    auto aug_body = augment(diffee->body());
    diff_lam->set_body(aug_body);

    //auto init_sink_lam = init_sinks();
    //auto init_sink_mem = end_mem();

    add_inverted(diffee->var(), diff_lam->var());

    current_state   = State::Invert;
    auto inv_diffee = invert(diffee)->as_nom<Lam>();

    //                 ______________
    //                |              |
    //backward_begin -+> inv_diffee -+> backward_begin->ret


    forward_end->set_body(w.app(diff_lam->ret_var(), merge_flat(forward_end->var(), backward_begin)));
    auto backward_end = w.nom_lam(inv_diffee->ret_pi(), w.dbg("backward_end_" + diffee->name()));
    backward_end->set_filter(true);

    auto ret_ret = diffee->ret_pi()->num_doms();
    auto inv_num_ret = backward_begin->num_doms() - 1;

    auto num_gradients = inv_num_ret - ret_ret;

    DefVec test;
    DefVec test2;

    size_t i = num_gradients;
    for( auto var : backward_begin->args() ){
        if(match<mem::M>(var->type()) || i <= 0){
            test.push_back(var);
        }else if(i > 0){
            i--;
        }
    }

    test.push_back(backward_end);

    backward_begin->set_body(w.app(inv_diffee, test));
    backward_end->set_body(w.app(backward_begin->ret_var(), backward_end->var()));

    return diff_lam;
}

} // namespace thorin::autodiff