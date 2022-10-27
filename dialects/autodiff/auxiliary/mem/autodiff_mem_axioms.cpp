#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

std::optional<const Def*> AutoDiffEval::handle_memory(const Def* def, Lam* f, Lam* f_diff) {
    if (auto lea = match<mem::lea>(def)) { return augment_lea(lea, f, f_diff); }
    if (auto load = match<mem::load>(def)) { return augment_load(load, f, f_diff); }
    if (auto store = match<mem::store>(def)) { return augment_store(store, f, f_diff); }
    if (auto malloc = match<mem::malloc>(def)) { return augment_malloc(malloc, f, f_diff); }
    if (auto alloc = match<mem::alloc>(def)) { return augment_alloc(alloc, f, f_diff); }
    if (auto bitcast = match<core::bitcast>(def)) { return augment_bitcast(bitcast, f, f_diff); }

    return std::nullopt;
}

const Def* AutoDiffEval::augment_lea(const App* lea, Lam* f, Lam* f_diff) {
    auto& w = world();

    auto [arr_ptr, idx] = lea->arg()->projs<2>();
    auto aug_ptr        = augment(arr_ptr, f, f_diff);
    auto aug_idx        = augment(idx, f, f_diff);

    auto aug_lea              = mem::op_lea(aug_ptr, aug_idx);
    partial_pullback[aug_lea] = zero_pullback_fun(aug_lea->type(), f);

    auto gradient_array = gradient_ptrs[aug_ptr];
    // no pullbacks, just need shadow information for ptr
    if (gradient_array) {
        gradient_ptrs[aug_lea] = mem::op_lea(gradient_array, aug_idx, w.dbg("pullback_lea"));
    } else {
        // TODO: incorporate gradient_ptrs in shadow, remove cases
        auto pullback_array = shadow_pullback[aug_ptr];
        assert(pullback_array);
        shadow_pullback[aug_lea] = mem::op_lea(pullback_array, aug_idx, w.dbg("pullback_lea"));
    }

    return aug_lea;
}

const Def* AutoDiffEval::augment_load(const App* load, Lam* f, Lam* f_diff) {
    auto& w = world();

    auto [load_mem, load_ptr] = load->args<2>();

    auto aug_mem = augment(load_mem, f, f_diff);
    auto aug_ptr = augment(load_ptr, f, f_diff);

    auto aug_load = mem::op_load(aug_mem, aug_ptr, w.dbg("aug_load"))->as<App>();

    auto aug_load_mem = aug_load->arg(1);

    auto gradient_ptr = gradient_ptrs[aug_ptr];
    if (gradient_ptr) {
        // for arrays with gradient array instead of loading pullbacks
        // we can specify a continuation which will add the gradient to the gradient array
        partial_pullback[aug_load] = create_gradient_collector(gradient_ptr, f);
        return aug_load;
    } else {
        // load pullback from shadow array
        auto pullback_ptr = shadow_pullback[aug_ptr];
        assert(pullback_ptr);
        auto [pullback_mem, pullback] = mem::op_load(aug_load_mem, pullback_ptr, w.dbg("pullback_load"))->projs<2>();
        partial_pullback[aug_load]    = pullback;
        return w.tuple({pullback_mem, aug_load});
    }
}

const Def* AutoDiffEval::augment_store(const App* store, Lam* f, Lam* f_diff) {
    auto& world = store->world();

    auto aug_arg                     = augment(store->arg(), f, f_diff);
    auto [aug_mem, aug_ptr, aug_val] = aug_arg->projs<3>();

    auto shadow_pb_ptr = shadow_pullback[aug_ptr];

    auto pb = partial_pullback[aug_val];
    // store the element pb
    auto pb_store = mem::op_store(aug_mem, shadow_pb_ptr, pb);
    // store the element in forward pass
    auto aug_store = mem::op_store(pb_store, aug_ptr, aug_val);
    return aug_store;
}

const Def* AutoDiffEval::augment_alloc(const App* alloc, Lam* f, Lam* f_diff) {
    // alloc: {T,as} -> Mem -> Mem * Ptr
    auto& world = alloc->world();
    // TODO: augment all arguments
    auto aug_mem = augment(alloc->arg(), f, f_diff);

    auto callee = alloc->callee()->as<App>();
    // TODO: think about higher order types
    auto type = callee->arg(0_s);

    // TODO: maybe reorder allocation order to inline pullback in flow
    auto [alloc_mem, alloc_ptr] = mem::op_alloc(type, aug_mem)->projs<2>();

    auto pb_ty = shadow_array_type(type, f->dom(0_s));
    auto [alloc_mem_2, pullback_ptr] =
        mem::op_malloc(pb_ty, alloc_mem, world.dbg(alloc->name() + "_pullback_alloc"))->projs<2>();

    allocated_memory.insert(pullback_ptr);
    // TODO: check if this should be gradient_ptrs
    shadow_pullback[alloc_ptr] = pullback_ptr;

    auto tup = world.tuple({alloc_mem_2, alloc_ptr});

    partial_pullback[tup]          = zero_pullback_fun(tup->type(), f);
    partial_pullback[alloc_mem_2]  = zero_pullback_fun(alloc_mem_2->type(), f);
    partial_pullback[pullback_ptr] = zero_pullback_fun(pullback_ptr->type(), f);

    return tup;
}

const Def* AutoDiffEval::augment_malloc(const App* malloc, Lam* f, Lam* f_diff) {
    auto aug_arg = augment(malloc->arg(), f, f_diff);
    // TODO: not yet implemented
    malloc->dump();
    assert(false && "not yet implemented");
    return malloc;
}

const Def* AutoDiffEval::augment_bitcast(const App* bitcast, Lam* f, Lam* f_diff) {
    auto& world  = bitcast->world();
    auto aug_arg = augment(bitcast->arg(), f, f_diff);

    auto dst              = core::op_bitcast(bitcast->type(), aug_arg);
    partial_pullback[dst] = zero_pullback_fun(dst->type(), f);
    return dst;
}

} // namespace thorin::autodiff
