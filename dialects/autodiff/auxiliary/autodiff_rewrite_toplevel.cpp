#include "dialects/affine/affine.h"
#include "dialects/affine/autogen.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/auxiliary/autodiff_dep_analysis.h"
#include "dialects/autodiff/auxiliary/autodiff_cache_analysis.h"
#include "dialects/autodiff/auxiliary/autodiff_flow_analysis.h"
#include "dialects/autodiff/auxiliary/autodiff_war_analysis.h"
#include "dialects/autodiff/builder.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/math/math.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

#include "thorin/analyses/domtree.h"

namespace thorin::autodiff {

void AutoDiffEval::assign_gradients(Lam* diffee, Lam* diff) {
    auto num        = diff->num_vars();
    size_t diffee_i = 0;
    size_t diff_i   = 0;
    for (; diff_i < num;) {
        auto def = diff->var(diff_i++);
        if (match<mem::Ptr>(def->type())) {
            auto values   = diffee->var(diffee_i);
            auto gradient = diff->var(diff_i++);

            gradient_pointers[values] = gradient;
        }
        diffee_i++;
    }
}

const Def* AutoDiffEval::input_mapping(Lam* forward) {
    auto& w  = world();
    auto num = forward->num_vars();
    size_t i = 0;
    DefVec var_mapping;
    for (; i < num;) {
        auto values = forward->var(i++);
        var_mapping.push_back(values);
        if (match<mem::Ptr>(values->type())) { i++; }
    }

    return w.tuple(var_mapping);
}

Lam* AutoDiffEval::free_memory() {
    auto free_memory = create_block("free_memory");
    ret(free_memory);
    return free_memory;
}

Lam* AutoDiffEval::init_caches(Lam* next) {
    auto& w        = world();
    Lam* current   = build(w).mem_ty().lam("init_caches");
    InitFrame last = {.lam = current, .mem = mem::mem_var(current)};
    for (; !init_frames.empty();) {
        auto next = init_frames.top();
        init_frames.pop();
        last.lam->set_body(w.app(next.lam, last.mem));
        last = next;
    }

    last.lam->set_body(w.app(next, last.mem));
    return current;
}



const Def* AutoDiffEval::derive_(const Def* def) {
    auto& w     = world();
    auto diffee = def->isa_nom<Lam>();
    assert(diffee);

    flow_analysis = std::make_unique<FlowAnalysis>();
    flow_analysis->run(diffee);
    cache_analysis = std::make_unique<CacheAnalysis>(*flow_analysis.get());
    cache_analysis->run();

    WARAnalysis war_analysis(diffee);



    auto diff_ty = autodiff_type_fun_pi(diffee->type());

    auto diff_lam = w.nom_lam(diff_ty, w.dbg("diff_lam_" + diffee->name()));
    diff_lam->set_filter(true);
    diff_lam->set_body(w.top_nat());

    auto forward_begin = w.nom_lam(w.cn(mem::type_mem(w)), w.dbg("forward_begin_" + diffee->name()));
    forward_begin->set_filter(true);
    forward_begin->set_body(w.top_nat());

    auto diff_ret_type  = diff_ty->dom(diff_ty->num_doms() - 1)->as<Pi>();
    auto inv_lam_ty     = diff_ret_type->dom(diff_ret_type->num_doms() - 1)->as<Pi>();
    auto backward_begin = w.nom_lam(inv_lam_ty, w.dbg("backward_begin_" + diffee->name()));
    backward_begin->set_filter(true);
    backward_begin->set_body(w.top_nat());

    auto lam_return_ty = diffee->type()->ret_pi();
    auto forward_end   = w.nom_lam(lam_return_ty, w.dbg("forward_end_" + diffee->name()));
    forward_end->set_filter(true);
    forward_end->set_body(w.top_nat());

    assign_gradients(diffee, diff_lam);
    auto mapping             = mask_last(input_mapping(diff_lam), forward_end);
    augmented[diffee->var()] = mapping;
    add_inverted(diffee->var(), mapping);

    current_state = State::Augment;
    init_mem(forward_begin);
    auto aug_body = augment(diffee->body());
    forward_begin->set_body(aug_body);

    current_state   = State::Invert;
    auto inv_diffee = invert_lam(diffee);

    forward_end->set_body(w.app(diff_lam->ret_var(), merge_flat(forward_end->var(), backward_begin)));
    auto backward_end = w.nom_lam(inv_diffee->ret_pi(), w.dbg("backward_end_" + diffee->name()));
    backward_end->set_body(w.top_nat());
    backward_end->set_filter(true);

    DefVec gradient_results;
    for (auto var : backward_begin->args()) { gradient_results.push_back(var); }
    gradient_results.push_back(backward_end);

    auto free_lam = free_memory();
    auto free_ret = create_ret(free_lam);

    backward_begin->set_body(w.app(inv_diffee, gradient_results));
    backward_end->set_body(w.app(free_lam, {mem::mem_var(backward_end), free_ret}));
    free_ret->set_body(w.app(backward_begin->ret_var(), mem::replace_mem(mem::mem_var(free_ret), backward_end->var())));

    diff_lam->set_body(w.app(init_caches(forward_begin), mem::mem_var(diff_lam)));

    return diff_lam;
}

} // namespace thorin::autodiff