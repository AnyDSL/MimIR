#include "dialects/affine/affine.h"
#include "dialects/affine/autogen.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/auxiliary/autodiff_cache_analysis.h"
#include "dialects/autodiff/auxiliary/autodiff_flow_analysis.h"
#include "dialects/autodiff/builder.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/math/math.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

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

void AutoDiffEval::mark(const Def* def) { markings.insert(def); }

const App* is_load_val(const Def* def) {
    if (auto extr = def->isa<Extract>()) {
        auto tuple = extr->tuple();
        if (auto load = match<mem::load>(tuple)) { return load; }
    }

    return nullptr;
}

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

bool contains_load(const Def* def) {
    if (def->isa<Var>()) return false;
    if (match<mem::load>(def)) return true;

    for (auto op : def->ops()) {
        if (contains_load(op)) { return true; }
    }

    return false;
}

void AutoDiffEval::scan(const Def* def) {
    if (!visited_scan.insert(def).second) return;

    if (auto rop = match<math::arith>(def)) {
        if (rop.id() == math::arith::mul) {
            mark(rop->arg(0));
            mark(rop->arg(1));
        } else if (rop.id() == math::arith::div) {
            mark(rop->arg(0));
            mark(rop->arg(1));
        }
    } else if (auto extrema = match<math::extrema>(def)) {
        if (extrema.id() == math::extrema::maximum || extrema.id() == math::extrema::minimum) {
            mark(extrema->arg(0));
            mark(extrema->arg(1));
        }
    }

    if (auto exp = match<math::exp>(def)) {
        if (exp.id() == math::exp::exp) {
            mark(exp);
        } else if (exp.id() == math::exp::log) {
            mark(exp->arg(0));
        }
    }

    if (auto lea = match<mem::lea>(def)) {
        auto index = lea->arg(1);
        if (contains_load(index)) { mark(index); }
    }

    if (auto gamma = match<math::gamma>(def)) { mark(gamma->arg(0)); }

    for (auto op : def->ops()) { scan(op); }
}

void AutoDiffEval::prepare(Lam* lam) {
    scan(lam);

    Scope scope(lam);
    for (auto mark : markings) {
        if (scope.bound(mark)) {
            if (auto load = is_load_val(mark)) {
                auto ptr = load->arg(1);
                if (has_op_store(ptr)) { requires_caching.insert(mark); }
            } else if (!mark->isa<Lit>()) {
                requires_caching.insert(mark);
            }
        }
    }
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

    FlowAnalysis flow_analysis;
    flow_analysis.run(diffee);
    CacheAnalysis cache_analysis(flow_analysis);
    cache_analysis.run();

    requires_caching = cache_analysis.targets();

    for (auto def : requires_caching) { def->dump(1); }

    auto size = requires_caching.size();

    // prepare(diffee);

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