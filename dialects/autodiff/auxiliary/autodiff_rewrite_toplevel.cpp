#include "dialects/affine/affine.h"
#include "dialects/affine/autogen.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/builder.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

bool has_op_store(const Def* def, DefSet& visited) {
    if (visited.contains(def)) return false;
    visited.insert(def);

    if (auto extr = def->isa<Extract>()) {
        if (has_op_store(extr->tuple(), visited)) { return true; }
    } else if (auto lea = match<mem::lea>(def)) {
        auto [arr_ptr, _] = lea->args<2>();

        if (has_op_store(arr_ptr, visited)) { return true; }
    } else if (match<mem::store>(def)) {
        return true;
    }

    if (match<mem::Ptr>(def->type()) || def->isa<Tuple>()) {
        for (auto use : def->uses()) {
            if (has_op_store(use, visited)) { return true; }
        }
    }

    return false;
}

bool has_op_store(const Def* ptr) {
    DefSet visited;
    return has_op_store(ptr, visited);
}

void AutoDiffEval::fetch_gradients(Lam* src, Lam* backward) {
    auto src_arg      = src->arg();
    auto backward_arg = backward->arg();

    size_t fwd_i = 0;
    size_t bwd_i = 0;
    for (auto def : src_arg->projs()) {
        if (match<mem::Ptr>(def->type())) {
            auto variable = src_arg->proj(fwd_i);
            auto gradient = backward_arg->proj(bwd_i);

            gradient_pointers[variable] = gradient;
            bwd_i++;
        } else if (match<mem::M>(def->type())) {
            bwd_i++;
        }

        fwd_i++;
    }
}

Lam* AutoDiffEval::free_memory() {
    auto& w          = world();
    auto free_memory = create_block("free_memory");
    for (auto ptr : allocated_memory) { op_free(ptr); }
    ret(free_memory);
    return free_memory;
}

void AutoDiffEval::mark(const Def* def) { markings.insert(def); }

void AutoDiffEval::scan(const Def* def) {
    if (!visited_scan.insert(def).second) return;

    if (auto rop = match<core::rop>(def)) {
        if (rop.id() == core::rop::mul || rop.id() == core::rop::div) {
            mark(rop->arg(0));
            mark(rop->arg(1));
        }
    }

    for (auto op : def->ops()) { scan(op); }
}

const App* is_load_val(const Def* def) {
    if (auto extr = def->isa<Extract>()) {
        auto tuple = extr->tuple();
        if (auto load = match<mem::load>(tuple)) { return load; }
    }

    return nullptr;
}

void AutoDiffEval::prepare(const Def* def) {
    scan(def);

    for (auto mark : markings) {
        mark->dump(1);
        if (auto load = is_load_val(mark)) {
            auto ptr = load->arg(1);
            if (has_op_store(ptr)) { requires_caching.insert(mark); }
        } else {
            requires_caching.insert(mark);
        }
    }
}

const Def* AutoDiffEval::derive_(const Def* def) {
    auto& w     = world();
    auto diffee = def->isa_nom<Lam>();
    assert(diffee);

    prepare(diffee);

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
    init_mem(diff_lam);
    auto aug_body = augment(diffee->body());
    diff_lam->set_body(aug_body);

    add_inverted(diffee->var(), diff_lam->var());

    current_state   = State::Invert;
    auto inv_diffee = invert_lam(diffee);

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

    auto free_lam = free_memory();
    auto free_ret = create_ret(free_lam);

    backward_begin->set_body(w.app(inv_diffee, gradient_results));
    backward_end->set_body(w.app(free_lam, {mem::mem_var(backward_end), free_ret}));
    free_ret->set_body(w.app(backward_begin->ret_var(), mem::replace_mem(mem::mem_var(free_ret), backward_end->var())));
    return diff_lam;
}

} // namespace thorin::autodiff