#include <algorithm>
#include <string>

#include "thorin/tuple.h"

#include "thorin/util/assert.h"

#include "dialects/affine/affine.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/autogen.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/builder.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

//#define todo 0

namespace thorin::autodiff {

#define f_arg_ty continuation_dom(f->type())

const Def* zero(World& w) { return w.lit_int(32, 0); }

const Def* one(World& w) { return w.lit_int(32, 1); }

Lam* chain(const Def* mem, Lam* lam, const Def* next) {
    auto& w       = mem->world();
    auto callback = build(w).mem_ty().lam(next->name() + "_callback");
    callback->set_body(w.app(lam->var(1_s), {mem::mem_var(callback)}));
    lam->set_body(w.app(next, {mem, callback}));
    return lam;
}

Lam* chain(const Def* first, const Def* second) {
    auto& w              = first->world();
    auto chain           = build(w).mem_ty().mem_cn().lam(first->name() + "_chain");
    auto callback_first  = build(w).mem_ty().lam(first->name() + "_callback");
    auto callback_second = build(w).mem_ty().lam(second->name() + "_callback");
    chain->set_body(w.app(first, {mem::mem_var(chain), callback_first}));
    callback_first->set_body(w.app(second, {mem::mem_var(callback_first), callback_second}));
    callback_second->set_body(w.app(chain->var(1_s), {mem::mem_var(callback_second)}));
    return chain;
}

Lam* AutoDiffEval::create_block(const std::string& name) {
    auto& w  = world();
    auto lam = build(w).mem_ty().mem_cn().with_filter().lam(name);
    init_mem(lam);
    return lam;
}

Lam* AutoDiffEval::create_return(const std::string& name) {
    auto& w  = world();
    auto lam = build(w).mem_ty().with_filter().lam("return_" + name);
    init_mem(lam);
    return lam;
}

Lam* AutoDiffEval::create_end(const std::string& name) {
    auto& w  = world();
    auto lam = create_block(name);
    ret(lam);
    return lam;
}

Lam* AutoDiffEval::create_invert_block(const std::string& name) { return create_block("invert_" + name); }

Lam* AutoDiffEval::create_invert_return(const std::string& name) { return create_return("invert_" + name); }

Lam* AutoDiffEval::create_invert_end() { return create_end("invert_end"); }

const Def* AutoDiffEval::augment_lit(const Lit* lit) {
    auto& w = world();

    auto aug_lit = lit;
    // set zero pullback
    // auto pb                   = zero_pullback(lit->type());
    // partial_pullback[aug_lit] = pb;
    return lit;
}

const Def* AutoDiffEval::augment_var(const Var* var) {
    auto& w = world();
    assert(false);
    // assert(partial_pullback.count(aug_var));
    return var;
}

const Def* AutoDiffEval::augment_lam(Lam* lam) {
    auto& w      = world();
    auto aug_lam = w.nom_lam(lam->type()->as<Pi>(), w.dbg("aug_" + lam->name()));
    aug_lam->set_filter(true);
    augmented[lam->var()] = aug_lam->var();
    augmented[lam]        = aug_lam;

    auto aug_body = augment(lam->body());
    aug_lam->set_body(aug_body);
    return aug_lam;
}

const Def* AutoDiffEval::augment_extract(const Extract* ext) {
    auto& world = ext->world();

    auto tuple = ext->tuple();
    auto index = ext->index();

    auto aug_tuple = augment(tuple);
    auto aug_index = augment(index);

    auto aug_ext = world.extract(aug_tuple, aug_index);

    return aug_ext;
}

const Def* AutoDiffEval::augment_tuple(const Tuple* tup) {
    auto& world = tup->world();

    // augment ops

    auto projs = tup->projs();
    // TODO: should use ops instead?
    DefArray aug_ops(projs, [&](const Def* op) { return augment(op); });

    auto aug_tup = world.tuple(aug_ops);

    return aug_tup;
}

const Def* AutoDiffEval::augment_pack(const Pack* pack) {
    // TODO:

    return pack;
}

Lam* AutoDiffEval::free_memory_lam() {
    auto& w = world();

    auto free = build(w).mem_ty().mem_cn().lam("free");

    auto mem = mem::mem_var(free);
    for (auto memory : allocated_memory) { mem = mem::op_free(mem, memory); }

    free->set_body(w.app(free->var(1_s), mem));
    return free;
}

const Lam* wrap_append_app(const Def* lam, const Def* call_after_lam) {
    auto& w = lam->world();

    auto lam_ty     = lam->type()->as<Pi>();
    auto lam_ret_ty = lam_ty->dom(1)->as<Pi>();

    auto wrapper = w.nom_lam(lam_ty, w.dbg(lam->name() + "_wrapper"));
    wrapper->set_filter(true);
    auto after_first = w.nom_lam(lam_ret_ty, w.dbg(lam->name() + "_after_lam"));
    after_first->set_filter(true);
    auto after_second = build(w).mem_ty().lam(lam->name() + "_after_suffix");
    after_second->set_filter(true);

    auto arg = mem::replace_mem(mem::mem_var(after_second), after_first->var());
    after_second->set_body(w.app(wrapper->var(1_s), arg));
    after_first->set_body(w.app(call_after_lam, {mem::mem_var(after_first), after_second}));
    wrapper->set_body(w.app(lam, {wrapper->var(0_s), after_first}));
    return wrapper;
}

const Def* AutoDiffEval::augment_app(const App* app) {
    auto& world = app->world();

    auto callee = app->callee();
    auto arg    = app->arg();

    callee->type()->dump();
    arg->type()->dump();

#ifdef todo

    bool caching = false;

    if (match<core::wrap::mul>(ax)) {
        auto value    = invert[app];
        invert[left]  = value * value_cache[right];
        invert[right] = value * value_cache[left];
    } else if (match<core::wrap::add>(ax)) {
        auto value    = invert[app];
        invert[left]  = value;
        invert[right] = value;
    }

#else

#endif

    auto aug_arg    = augment(arg);
    auto aug_callee = augment(callee);
    // auto arg_ppb    = partial_pullback[aug_arg];

    return world.app(aug_callee, aug_arg);
}

const Def* AutoDiffEval::augment_lea(const App* lea) {
    auto& w = world();

    auto [arr_ptr, idx] = lea->arg()->projs<2>();
    auto aug_ptr        = augment(arr_ptr);
    auto aug_idx        = augment(idx);

    auto aug_lea = mem::op_lea(aug_ptr, aug_idx, arr_ptr->dbg());

    return aug_lea;
}

const Def* AutoDiffEval::create_init_frame(const std::string& name, std::function<const Def*(const Def*)> func) {
    auto& w  = world();
    Lam* lam = build(w).mem_ty().lam(name);
    auto mem = func(mem::mem_var(lam));
    init_frames.push(InitFrame{.lam = lam, .mem = mem});
}

const Def* AutoDiffEval::create_init_alloc_frame(const std::string& name, const Def* alloc_ty) {
    const Def* ptr = nullptr;
    create_init_frame("alloc_" + name, [&](const Def* mem) {
        auto& w                                 = mem->world();
        auto alloc_cache                        = mem::op_malloc(alloc_ty, mem, w.dbg(name));
        auto [alloc_cache_mem, alloc_cache_ptr] = alloc_cache->projs<2>();
        ptr                                     = alloc_cache_ptr;
        return alloc_cache_mem;
    });

    return ptr;
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

const Def* AutoDiffEval::wrap_cache(const App* load, const App* aug_load) {
    auto& w = world();

    auto [_, load_ptr] = load->args<2>();
    load_ptr->dump(2);
    bool has_store = has_op_store(load_ptr);

    std::string name = "cache";

    const Def* cache_ptr      = aug_load->arg(1);
    auto [load_mem, load_val] = aug_load->projs<2>();
    auto result_mem           = load_mem;

    load_ptr->dump(2);
    std::cout << has_store << std::endl;
    if (has_store || true) {
        auto loop_size   = current_loop->size;
        auto cache_index = current_loop->cache_index();

        auto loop_size_nat = core::op_bitcast(w.type_nat(), loop_size);

        auto arr_ty = w.arr(loop_size_nat, load_val->type());

        auto alloc_cache_ptr = create_init_alloc_frame(name, arr_ty);

        auto unsized_arr_ptr_ty = mem::type_ptr(w.arr(w.top(w.type_nat()), load_val->type()));
        cache_ptr               = core::op_bitcast(unsized_arr_ptr_ty, alloc_cache_ptr, w.dbg(name));

        auto cache_lea                   = mem::op_lea(cache_ptr, cache_index, w.dbg("lea_" + name));
        result_mem                       = mem::op_store(load_mem, cache_lea, load_val, w.dbg("store_" + name));
        cache_map[load]                  = cache_ptr;
        cache_loop_assignment[cache_ptr] = current_loop;
    }

    return w.tuple({result_mem, load_val});
}

const Def* AutoDiffEval::augment_load(const App* load) {
    auto& w = world();

    auto [load_mem, load_ptr] = load->args<2>();

    auto aug_mem = augment(load_mem);
    auto aug_ptr = augment(load_ptr);

    loads_.insert(load);

    auto aug_load  = mem::op_load(aug_mem, aug_ptr, w.dbg(aug_ptr->name() + "_val"))->as<App>();
    auto wrap_load = wrap_cache(load, aug_load);

    return wrap_load;
}

const Def* AutoDiffEval::augment_store(const App* store) {
    auto& world = store->world();

    auto aug_arg                     = augment(store->arg());
    auto [aug_mem, aug_ptr, aug_val] = aug_arg->projs<3>();

    // auto shadow_pb_ptr = shadow_pullbacks[aug_ptr];

    // auto pb = partial_pullback[aug_val];
    // auto pb_store = mem::op_store(aug_mem, shadow_pb_ptr, pb);
    auto aug_store = mem::op_store(aug_mem, aug_ptr, aug_val, store->dbg());
    return aug_store;
}

const Def* AutoDiffEval::zero_pullback(const Def* domain) {
    const Def* A   = f_arg_ty;
    auto& world    = A->world();
    auto A_tangent = tangent_type_fun(A);
    auto pb_ty     = pullback_type(domain, A);
    auto pb        = world.nom_lam(pb_ty, world.dbg("zero_pb"));
    pb->app(true, pb->var(1), autodiff_zero(mem::mem_var(pb)));
    return pb;
}

const Def* AutoDiffEval::get_pullback(const Def* op) {
    auto pb = partial_pullback[op];
    if (!pb) { return zero_pullback(op->type()); }
    return pb;
}

const Def* AutoDiffEval::augment_alloc(const App* alloc) {
    auto org_ptr = alloc->proj(1);
    auto aug_mem = augment(alloc->arg(0_s));

    auto callee = alloc->callee()->as<App>();
    auto type   = callee->arg(0_s);

    auto aug_alloc = mem::op_alloc(type, aug_mem);
    auto value_ptr = aug_alloc->proj(1);

    auto gradient_ptr = create_init_alloc_frame("gradient_arr", type);

    // add to list of allocated memory
    allocated_memory.insert(gradient_ptr);
    gradient_ptrs[org_ptr] = gradient_ptr;

    return aug_alloc;
}

const Def* AutoDiffEval::augment_malloc(const App* malloc) {
    auto aug_arg = augment(malloc->arg());
    // TODO: not yet implemented
    malloc->dump();
    malloc->dump();
    return malloc;
}

const Def* AutoDiffEval::augment_bitcast(const App* bitcast) {
    auto& world  = bitcast->world();
    auto aug_arg = augment(bitcast->arg());

    auto dst = core::op_bitcast(bitcast->type(), aug_arg, bitcast->arg()->dbg());
    return dst;
}

const Def* for_size(const Def*& mem, const Def* start, const Def* end, const Def* inc) {
    auto& w            = start->world();
    auto span          = core::op(core::wrap::sub, core::WMode::none, end, start);
    auto inc_minus_one = core::op(core::wrap::sub, core::WMode::none, inc, one(w));
    auto span_ext      = core::op(core::wrap::add, core::WMode::none, span, inc_minus_one);
    auto [mem2, size]  = core::op(core::div::udiv, mem, span_ext, inc)->projs<2>();
    mem                = mem2;
    return size;
}

const Def* end_index(const Def*& mem, const Def* start, const Def* end, const Def* inc) {
    auto span             = core::op(core::wrap::sub, core::WMode::none, end, start);
    auto [mem2, repeats]  = core::op(core::div::udiv, mem, span, inc)->projs<2>();
    mem                   = mem2;
    auto discretized_span = core::op(core::wrap::mul, core::WMode::none, repeats, inc);
    auto end_index        = core::op(core::wrap::add, core::WMode::none, discretized_span, start);
    return end_index;
}

const Def* normalized_to_loop_index(const Def* start, const Def* inc, const Def* normalized_index) {
    auto correct_increment = core::op(core::wrap::mul, core::WMode::none, normalized_index, inc);
    auto loop_index        = core::op(core::wrap::add, core::WMode::none, correct_increment, start);
    return loop_index;
}

const Def* replace(const Def* tup, int index, const Def* replacement) {
    auto& w = tup->world();
    assert(tup->num_projs() > 1);
    assert(tup->num_projs() >= index + 1);
    auto projs   = tup->projs();
    projs[index] = replacement;
    return w.tuple(projs);
}

#ifdef todo

#else

#endif

const Def* AutoDiffEval::invert_lam(Lam* lam_def) {
    auto& w = world();

    auto lam = lam_def->isa_nom<Lam>();
    assert(lam);

    auto inv_body_lam = invert(lam->body());

    /*
    const Def* new_app = nullptr;
    if( auto app = lam_def->isa<App>() ){
        auto callee = app->callee();
        auto arg    = app->arg();

        auto call_mem = end_mem();

        auto invert_callee = invert(callee);
        auto invert_arg = invert(arg);

        auto combined = chain(invert_callee, invert_arg);

        new_app = w.app(combined, mem);
    }*/

    // auto invert_lam = create_invert_block("lam");
    // invert_lam->set_body(new_app);
    // return chain(end_mem(), invert_lam, invert_lam);

    return inv_body_lam;
}

const Def* AutoDiffEval::normalized_to_cache_index(const Def* normalized_index) {
    const Def* cache_index;
    auto parent = current_loop->parent;
    if (parent != nullptr) {
        cache_index = core::op(core::wrap::mul, core::WMode::none, parent->size, normalized_index);
        cache_index = core::op(core::wrap::add, core::WMode::none, parent->index(), cache_index);
    } else {
        cache_index = normalized_index;
    }

    return cache_index;
}

const Def* AutoDiffEval::invert_for_body(const App* for_app) {
    auto& w = world();

    auto [start, end, inc, acc, body, exit] = for_app->args<6>();

    auto aug_start = augment(start);
    auto aug_inc   = augment(inc);

    auto loop_body = body->isa_nom<Lam>();
    auto raw_index = loop_body->var(0_s);

    assert(loop_body);

    Lam* inv_loop_body = w.nom_lam(loop_body->type()->as<Pi>(), w.dbg("invert_" + loop_body->name()));
    inv_loop_body->set_filter(true);

    current_loop->size->dump();
    const Def* size_sub_one = core::op(core::wrap::sub, core::WMode::none, current_loop->local_size, one(w));

    auto normalized_index = inv_loop_body->var(0_s);
    normalized_index      = core::op(core::wrap::sub, core::WMode::none, size_sub_one, normalized_index);

    current_loop->index()       = normalized_to_loop_index(aug_start, aug_inc, normalized_index);
    current_loop->cache_index() = normalized_to_cache_index(normalized_index);

    add_inverted(raw_index, current_loop->index());
    add_inverted(loop_body->var(loop_body->num_vars() - 1), create_invert_end());
    add_inverted(mem::mem_var(loop_body), create_invert_end());
    add_inverted(loop_body, inv_loop_body);
    auto body_app = loop_body->body();

    auto inv_body = invert(body_app);

    auto body_mem      = mem::mem_var(inv_loop_body);
    auto body_continue = inv_loop_body->var(inv_loop_body->num_vars() - 1);
    inv_loop_body->set_body(w.app(inv_body, {body_mem, body_continue}));

    return inv_loop_body;
}

const Def* AutoDiffEval::invert_for(const App* for_app) {
    auto& w = world();

    auto [start, end, inc, acc, body, exit] = for_app->args<6>();

    auto aug_start = augment(start);
    auto aug_end   = augment(end);
    auto aug_inc   = augment(inc);

    auto invert_for     = create_invert_block("for");
    auto invert_start   = zero(w);
    auto invert_inc     = one(w);
    auto size           = for_size(current_mem, aug_start, aug_end, aug_inc);
    auto invert_end     = size;
    auto invert_for_mem = end_mem();

    // for_size(current_mem, aug_start, aug_end, aug_inc);

    push_loop_frame(for_app, size);
    auto invert_body = invert_for_body(for_app);
    pop_loop_frame();
    auto invert_exit = invert(exit);

    auto return_exit     = create_invert_return("exit");
    auto return_exit_mem = end_mem();

    auto return_for     = create_invert_return("for");
    auto return_for_mem = end_mem();

    auto return_entry     = create_invert_return("for_entry");
    auto return_entry_mem = end_mem();

    auto invert_entry = invert(mem::mem_def(acc));

    return_entry->set_body(w.app(invert_for->var(1_s), return_entry_mem));
    return_for->set_body(w.app(invert_entry, {return_for_mem, return_entry}));
    return_exit->set_body(
        affine::op_for(w, invert_start, invert_end, invert_inc, {return_exit_mem}, invert_body, return_for));
    invert_for->set_body(w.app(invert_exit, {invert_for_mem, return_exit}));

    return invert_for;
}

void AutoDiffEval::push_loop_frame(const App* for_app, const Def* size) {
    if (loop_assignment.contains(for_app)) {
        current_loop = loop_assignment[for_app];
    } else {
        auto next_frame = std::make_shared<LoopFrame>(*this);
        if (current_loop != nullptr) {
            next_frame->parent = current_loop;
            next_frame->size   = core::op(core::wrap::mul, core::WMode::none, current_loop->size, size);
        } else {
            next_frame->size = size;
        }

        next_frame->local_size   = size;
        loop_assignment[for_app] = next_frame;
        current_loop             = next_frame;
    }
}

void AutoDiffEval::pop_loop_frame() { current_loop = current_loop->parent; }

const Def* AutoDiffEval::augment_for_body(const Def* body, const Def* start, const Def* inc) {
    auto& w = world();

    auto loop_body = body->as_nom<Lam>();
    Lam* aug_body  = w.nom_lam(loop_body->type()->as<Pi>(), w.dbg("aug_" + loop_body->name()));
    aug_body->set_filter(true);

    auto org_index        = loop_body->var(0_s);
    auto normalized_index = aug_body->var(0_s);
    current_loop->index() = normalized_to_loop_index(start, inc, normalized_index);

    index_map[org_index]        = current_loop->index();
    augmented[loop_body->var()] = replace(aug_body->var(), 0, current_loop->index());
    augmented[loop_body]        = aug_body;

    current_loop->cache_index() = normalized_to_cache_index(normalized_index);
    aug_body->set_body(augment(loop_body->body()));

    return aug_body;
}

const Def* AutoDiffEval::augment_for(const App* for_app) {
    auto& w                                 = world();
    auto [start, end, inc, acc, body, exit] = for_app->args<6>();

    auto aug_start = augment(start);
    auto aug_end   = augment(end);
    auto aug_inc   = augment(inc);
    auto aug_acc   = augment(acc);

    auto mem = mem::mem_def(aug_acc);

    auto size = for_size(mem, aug_start, aug_end, aug_inc);
    push_loop_frame(for_app, size);
    auto aug_body = augment_for_body(body, aug_start, aug_inc);
    pop_loop_frame();
    auto aug_exit = augment(exit);

    if (current_loop == nullptr) {
        Lam* current   = build(w).mem_ty().lam("init_caches");
        InitFrame last = {.lam = current, .mem = mem::mem_var(current)};
        for (; !init_frames.empty();) {
            auto next = init_frames.top();
            init_frames.pop();
            last.lam->set_body(w.app(next.lam, last.mem));
            // last.lam->set(next.lam->reduce(last.mem));
            last = next;
        }

        aug_acc = mem::replace_mem(last.mem, aug_acc);
        last.lam->set_body(affine::op_for(w, zero(w), size, one(w), aug_acc->projs(), aug_body, aug_exit));
        return w.app(current, {mem});
    }

    aug_acc = mem::replace_mem(mem, aug_acc);
    return affine::op_for(w, zero(w), size, one(w), aug_acc->projs(), aug_body, aug_exit);
}

/// rewrite the given definition
///
const Def* AutoDiffEval::augment_(const Def* def) {
    auto& w = world();
    // for types holds:
    // we use macros above to avoid recomputation
    // TODO: alternative: use class instance to rewrite inside a function and save such values (f, f_diff, f_arg_ty)

    w.DLOG("Augment def {} : {}", def, def->type());

    if (auto for_app = match<affine::For>(def)) { return augment_for(for_app); }

    if (auto lea = match<mem::lea>(def)) { return augment_lea(lea->as<App>()); }

    if (auto load = match<mem::load>(def)) { return augment_load(load->as<App>()); }

    if (auto store = match<mem::store>(def)) { return augment_store(store->as<App>()); }

    if (auto malloc = match<mem::malloc>(def)) { return augment_malloc(malloc->as<App>()); }

    if (auto alloc = match<mem::alloc>(def)) { return augment_alloc(alloc->as<App>()); }

    if (auto bitcast = match<core::bitcast>(def)) { return augment_bitcast(bitcast->as<App>()); }

    // app => cont, operator, function
    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto arg    = app->arg();
        w.DLOG("Augment application: app {} with {}", callee, arg);
        return augment_app(app);
    }

    // projection
    else if (auto ext = def->isa<Extract>()) {
        // world.DLOG("Augment extract: {}", def);
        auto tuple = ext->tuple();
        auto index = ext->index();
        w.DLOG("Augment extract: {} #[{}]", tuple, index);
        return augment_extract(ext);
    }

    // vars (function argument)
    else if (auto var = def->isa<Var>()) {
        w.DLOG("Augment variable: {}", var);
        return augment_var(var);
    }

    // lam
    else if (auto lam = def->isa_nom<Lam>()) {
        w.DLOG("Augment nom lambda: {}", lam);
        return augment_lam(lam);
    } else if (auto lam = def->isa<Lam>()) {
        w.ELOG("Augment lambda: {}", lam);
        assert(false && "can not handle non-nominal lambdas");
    }

    // constants
    else if (auto lit = def->isa<Lit>()) {
        w.DLOG("Augment literal: {}", def);
        return augment_lit(lit);
    }

    // tuple
    else if (auto tup = def->isa<Tuple>()) {
        w.DLOG("Augment tuple: {}", def);
        return augment_tuple(tup);
    }

    // pack
    else if (auto pack = def->isa<Pack>()) {
        // TODO: nom pack
        auto shape = pack->arity(); // TODO: arity vs shape
        auto body  = pack->body();
        w.DLOG("Augment pack: {} : {} with {}", shape, shape->type(), body);
        return augment_pack(pack);
    }

    else if (auto axiom = def->isa<Axiom>()) {
        return axiom;
    }

    // for axiom app
    // else if (auto pi = def->isa<Pi>()) {
    // }

    // TODO: remaining (lambda, axiom)

    w.ELOG("did not expect to augment: {} : {}", def, def->type());
    w.ELOG("node: {}", def->node_name());
    assert(false && "augment not implemented on this def");
}

const Def* AutoDiffEval::invert_(const Def* def) {
    auto& w = world();
    // for types holds:
    // we use macros above to avoid recomputation
    // TODO: alternative: use class instance to rewrite inside a function and save such values (f, f_diff, f_arg_ty)

    w.DLOG("Augment def {} : {}", def, def->type());

    if (auto for_app = match<affine::For>(def)) { return invert_for(for_app); }

    if (auto lea = match<mem::lea>(def)) { return invert_lea(lea->as<App>()); }

    if (auto load = match<mem::load>(def)) { return invert_load(load->as<App>()); }

    if (auto store = match<mem::store>(def)) { return invert_store(store->as<App>()); }

    if (auto malloc = match<mem::malloc>(def)) { return invert_malloc(malloc->as<App>()); }

    if (auto alloc = match<mem::alloc>(def)) { return invert_alloc(alloc->as<App>()); }

    if (auto bitcast = match<core::bitcast>(def)) { return invert_bitcast(bitcast->as<App>()); }

    // app => cont, operator, function
    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto arg    = app->arg();
        w.DLOG("Augment application: app {} with {}", callee, arg);
        return invert_app(app);
    }

    // projection
    else if (auto ext = def->isa<Extract>()) {
        // world.DLOG("Augment extract: {}", def);
        auto tuple = ext->tuple();
        auto index = ext->index();
        w.DLOG("Augment extract: {} #[{}]", tuple, index);
        return invert_extract(ext);
    }

    // vars (function argument)
    else if (auto var = def->isa<Var>()) {
        w.DLOG("Augment variable: {}", var);
        return invert_var(var);
    }

    // lam
    else if (auto lam = def->isa_nom<Lam>()) {
        w.DLOG("Augment nom lambda: {}", lam);
        return invert_lam(lam);
    } else if (auto lam = def->isa<Lam>()) {
        w.ELOG("Augment lambda: {}", lam);
        assert(false && "can not handle non-nominal lambdas");
    }

    // constants
    else if (auto lit = def->isa<Lit>()) {
        w.DLOG("Augment literal: {}", def);
        return invert_lit(lit);
    }

    // tuple
    else if (auto tup = def->isa<Tuple>()) {
        w.DLOG("Augment tuple: {}", def);
        return invert_tuple(tup);
    } else if (auto ax = def->isa<Axiom>()) {
        return ax;
    }
}

const Def* AutoDiffEval::invert_var(const Var* var) { return create_invert_end(); }
const Def* AutoDiffEval::invert_extract(const Extract* extract) {
    auto tup   = extract->tuple();
    auto index = extract->index();

    // assert(!tup->isa<Var>());
    if (tup->isa<Var>()) { return create_invert_end(); }

    auto test = invert(tup);

    if (auto lam = test->isa_nom<Lam>()) { return test; }

    return extract;
}
const Def* AutoDiffEval::invert_app(const App* app) {
    auto& w = world();

    auto callee = app->callee();
    auto arg    = app->arg();

    auto invert_callee = invert(callee);
    auto invert_arg    = invert(arg);

    return chain(invert_callee, invert_arg);
}
const Def* AutoDiffEval::invert_lit(const Lit* lit) { return create_invert_end(); }
const Def* AutoDiffEval::invert_tuple(const Tuple* tuple) { return tuple; }
const Def* AutoDiffEval::invert_pack(const Pack* pack) { return pack; }
const Def* AutoDiffEval::invert_lea(const App* lea) {
    auto [ptr, index] = lea->args<2>();

    auto inv_index = resolve(index);

    auto gradient_arr = gradient_ptrs[ptr];
    auto gradient_ptr = mem::op_lea(gradient_arr, inv_index);

    // return mem::op_lea(inv_ptr, inv_index);
    return gradient_ptr;
}

const Def* AutoDiffEval::invert_load(const App* load) {
    auto& w         = world();
    auto [_, value] = load->projs<2>();
    auto [mem, ptr] = load->args<2>();

    auto gradient_ptr = invert(ptr);

    auto invert_load = create_invert_block("load");

    auto last_gradient = op_load(gradient_ptr);
    auto sink          = gradient_sinks[load];
    auto sink_value    = op_load(sink);
    auto next_gradient = core::op(core::wrap::add, core::WMode::none, last_gradient, sink_value);
    op_store(gradient_ptr, next_gradient);
    op_store(sink, zero(w));

    auto last_mem    = end_mem();
    auto invert_next = invert(mem);
    return chain(last_mem, invert_load, invert_next);
}

const Def* AutoDiffEval::op_load(const Def* ptr, bool with_mem) {
    assert(match<mem::M>(current_mem->type()));
    auto load              = mem::op_load(current_mem, ptr);
    auto [next_mem, value] = load->projs<2>();
    current_mem            = next_mem;
    if (with_mem) { return load; }

    return value;
}

void AutoDiffEval::op_store(const Def* ptr, const Def* value) { current_mem = mem::op_store(current_mem, ptr, value); }

void AutoDiffEval::op_free(const Def* ptr) { current_mem = mem::op_free(current_mem, ptr); }

const Def* AutoDiffEval::op_slot(const Def* type, const std::string& name) {
    auto& w                = world();
    auto [next_mem, value] = mem::op_slot(type, current_mem, w.dbg(name))->projs<2>();
    current_mem            = next_mem;
    return value;
}

void AutoDiffEval::ret(Lam* lam) {
    auto& w = world();
    lam->set_body(w.app(lam->var(1_s), end_mem()));
}

const Def* AutoDiffEval::resolve(const Def* def) {
    auto& w = world();

    if (def->isa<Axiom>()) { return def; }

    if (auto load = match<mem::load>(def)) {
        auto cache_arr = cache_map[def];
        const Def* cache_ptr;
        if (!cache_arr) {
            auto ptr  = load->arg(1);
            cache_ptr = resolve(ptr);
        } else {
            auto loop        = cache_loop_assignment[cache_arr];
            auto cache_index = loop->cache_index();
            cache_ptr        = mem::op_lea(cache_arr, cache_index);
        }
        auto grad_val = op_load(cache_ptr, true);
        return grad_val;
    }

    auto inv_def = inverted[def];
    if (inv_def) { return inv_def; }

    assert(!def->isa<Var>());

    auto new_ops = DefArray(def->num_ops(), [&](auto i) { return resolve(def->op(i)); });

    auto new_def = def->rebuild(w, def->type(), new_ops, def->dbg());

    return new_def;
}

void AutoDiffEval::propagate(const Def* target, const Def* gradient) {
    if (auto extract = target->isa<Extract>()) {
        if (auto load = match<mem::load>(extract->tuple())) {
            auto sink          = gradient_sinks[load];
            auto current_value = op_load(sink);
            auto sum           = core::op(core::wrap::add, core::WMode::none, current_value, gradient);
            op_store(sink, sum);
            return;
        }
    }

    if (auto wrap = match<core::wrap>(target)) {
        auto [left, right] = wrap->args<2>();

        if (wrap.id() == core::wrap::add) {
            propagate(left, gradient);
            propagate(right, gradient);
        } else if (wrap.id() == core::wrap::sub) {
            propagate(left, gradient);
            propagate(right, core::op_wminus(core::WMode::none, gradient));
        } else if (wrap.id() == core::wrap::mul) {
            auto left_value  = resolve(left);
            auto right_value = resolve(right);

            auto left_gradient  = core::op(core::wrap::mul, core::WMode::none, gradient, right_value);
            auto right_gradient = core::op(core::wrap::mul, core::WMode::none, gradient, left_value);

            propagate(left, left_gradient);
            propagate(right, right_gradient);
        }
    } else if (auto div = match<core::div>(target)) {
        auto [mem, left, right] = div->args<3>();
        if (div.id() == core::div::sdiv || div.id() == core::div::udiv) {
            auto left_value  = resolve(left);
            auto right_value = resolve(right);

            auto [div_mem, left_gradient] = core::op(div.id(), core::WMode::none, gradient, right_value)->projs<2>();
            auto right_gradient           = core::op(core::wrap::mul, core::WMode::none,
                                                     core::op_wminus(core::WMode::none, left_gradient), left_value);
            current_mem                   = div_mem;
            propagate(left, left_gradient);
            propagate(right, right_gradient);
        }
    }
}

const Def* AutoDiffEval::invert_store(const App* store) {
    auto& w                = world();
    auto [mem, ptr, value] = store->args<3>();

    auto invert_store = create_invert_block("store");

    auto grad_ptr = invert(ptr);

    auto load_gradient = op_load(grad_ptr);
    op_store(grad_ptr, zero(w));

    // push gradients to target
    propagate(value, load_gradient);

    auto next_mem = end_mem();
    // get next mem block which will be executed after this.
    auto invert_next = invert(mem);

    return chain(next_mem, invert_store, invert_next);
}
const Def* AutoDiffEval::invert_malloc(const App* malloc) { return malloc; }
const Def* AutoDiffEval::invert_alloc(const App* alloc) {
    auto [mem, alloc_ptr] = alloc->projs<2>();
    auto callee           = alloc->callee()->as<App>();
    auto type             = callee->arg(0_s);

    // prepends decicion when memory will be allocated by wrapping in lam.
    // auto gradient_ptr = create_init_alloc_frame("gradient_arr", type);

    // add to list of allocated memory
    // allocated_memory.insert(gradient_ptr);
    // gradient_ptrs[alloc_ptr] = gradient_ptr;

    return invert(mem);
}
const Def* AutoDiffEval::invert_bitcast(const App* bitcast) {
    auto& w      = world();
    auto new_ops = DefArray(bitcast->num_ops(), [&](auto i) { return invert(bitcast->op(i)); });

    auto new_def = bitcast->rebuild(w, bitcast->type(), new_ops, bitcast->dbg());

    return new_def;
}

} // namespace thorin::autodiff