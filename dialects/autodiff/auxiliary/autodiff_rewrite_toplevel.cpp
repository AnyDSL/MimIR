#include "dialects/affine/affine.h"
#include "dialects/affine/autogen.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/builder.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

void AutoDiffEval::fetch_gradients(Lam* src, Lam* backward) {
    auto src_arg      = src->arg();
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

Lam* AutoDiffEval::free_memory() {
    auto& w = world();

    auto free_memory = create_block("free_memory");

    for (auto ptr : allocated_memory) { op_free(ptr); }

    ret(free_memory);
    return free_memory;
}

Lam* strip_eta(Lam* lam) {
    if (auto app = lam->body()->isa<App>()) {
        app->arg()->dump(1);
        lam->var()->dump(1);
        if (app->arg() == lam->var()) {
            auto called_lam = app->callee()->as_nom<Lam>();
            return strip_eta(called_lam);
        }
    }

    return lam;
}

/// side effect: register pullback
const Def* AutoDiffEval::derive_(const Def* def) {
    auto& w     = world();
    auto diffee = strip_eta(def->isa_nom<Lam>());
    assert(diffee);

    auto diff_ty = autodiff_type_fun_pi(diffee->type());

    auto diff_lam = w.nom_lam(diff_ty, w.dbg("forward_begin_" + diffee->name()));
    diff_lam->set_filter(true);

    auto inv_lam_ty     = forward_to_backward(diffee->type()->as<Pi>());
    auto backward_begin = w.nom_lam(inv_lam_ty, w.dbg("backward_begin_" + diffee->name()));
    backward_begin->set_filter(true);
    backward_begin->set_body(w.top_nat());

    auto lam_return_ty = diffee->type()->ret_pi();
    auto forward_end   = w.nom_lam(lam_return_ty, w.dbg("forward_end_" + diffee->name()));
    forward_end->set_filter(true);
    forward_end->set_body(w.top_nat());

    augmented[diffee->var()] = mask_last(diff_lam->var(), forward_end);

    fetch_gradients(diffee, backward_begin);

    current_state = State::Augment;
    auto aug_body = augment(diffee->body());
    diff_lam->set_body(aug_body);

    // auto init_sink_lam = init_sinks();
    // auto init_sink_mem = end_mem();

    add_inverted(diffee->var(), diff_lam->var());

    current_state   = State::Invert;
    auto inv_diffee = invert_lam(diffee);

    //                 ______________
    //                |              |
    // backward_begin -+> inv_diffee -+> backward_begin->ret

    forward_end->set_body(w.app(diff_lam->ret_var(), merge_flat(forward_end->var(), backward_begin)));
    auto backward_end = w.nom_lam(inv_diffee->ret_pi(), w.dbg("backward_end_" + diffee->name()));
    backward_end->set_body(w.top_nat());
    backward_end->set_filter(true);

    auto ret_ret     = diffee->ret_pi()->num_doms();
    auto inv_num_ret = backward_begin->num_doms() - 1;

    auto num_gradients = inv_num_ret - ret_ret;

    DefVec gradient_results;
    size_t i = num_gradients;
    for (auto var : backward_begin->args()) {
        if (match<mem::M>(var->type()) || i <= 0) {
            gradient_results.push_back(var);
        } else {
            i--;
        }
    }

    gradient_results.push_back(backward_end);
    backward_begin->set_body(w.app(inv_diffee, gradient_results));
    backward_end->set_body(w.app(backward_begin->ret_var(), backward_end->var()));

    return diff_lam;
}

} // namespace thorin::autodiff