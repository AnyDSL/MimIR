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

namespace thorin::autodiff {

#define f_arg_ty continuation_dom(f->type())

const Def* zero(const Def* ty) {
    auto& w = ty->world();
    if (match<core::Real>(ty)) { return w.lit(ty, 0.0); }
    return w.lit(ty, 0);
}
const Def* zero(World& w) { return w.lit_int(32, 0); }
const Def* one(World& w) { return w.lit_int(32, 1); }

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

Lam* AutoDiffEval::create_lam(const Def* cn, const std::string& name) {
    auto& w  = world();
    auto lam = w.nom_lam(cn->as<Pi>(), w.dbg(name));
    lam->set_filter(true);
    init_mem(lam);
    return lam;
}

Lam* AutoDiffEval::create_end(const std::string& name) {
    auto& w  = world();
    auto lam = create_block(name);
    ret(lam);
    return lam;
}

void AutoDiffEval::ret(Lam* lam) {
    auto& w = world();
    lam->set_body(w.app(lam->ret_var(), end_mem()));
}

void deep_compare(const Def* src, const Def* dst, std::function<void(const Def*, const Def*)> differ) {
    auto num_projs = src->num_projs();
    assert(num_projs == dst->num_projs());

    if (num_projs > 1) {
        for (size_t i = 0; i < num_projs; i++) { deep_compare(src->proj(i), dst->proj(i), differ); }
    }

    differ(src, dst);
}

const Def* AutoDiffEval::augment_lit(const Lit* lit) {
    auto& w = world();
    return lit;
}

const Def* AutoDiffEval::augment_var(const Var* var) {
    assert(false);
    return var;
}

const Def* AutoDiffEval::augment_lam(Lam* lam) {
    auto& w = world();


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
    auto& w = world();
    DefArray aug_ops(tup->ops(), [&](const Def* op) { return augment(op); });
    return w.tuple(aug_ops);
}

const Def* AutoDiffEval::augment_pack(const Pack* pack) {
    auto& w    = world();
    auto shape = augment(pack->shape());
    auto body  = augment(pack->body());
    return w.pack(shape, body);
}

Lam* AutoDiffEval::free_memory_lam() {
    auto& w = world();

    auto free = build(w).mem_ty().mem_cn().lam("free");

    auto mem = mem::mem_var(free);
    for (auto memory : allocated_memory) { mem = mem::op_free(mem, memory); }

    free->set_body(w.app(free->ret_var(), mem));
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
    after_second->set_body(w.app(wrapper->ret_var(), arg));
    after_first->set_body(w.app(call_after_lam, {mem::mem_var(after_first), after_second}));
    wrapper->set_body(w.app(lam, {wrapper->arg(), after_first}));
    return wrapper;
}

const Def* AutoDiffEval::augment_app(const App* app) {
    auto& world = app->world();

    auto callee = app->callee();
    auto arg    = app->arg();

    auto aug_arg = augment(arg);

    if (auto lam = callee->isa_nom<Lam>()) {
        deep_compare(arg, lam->var(), [&](const Def* src, const Def* dst) { gradient_ptrs[dst] = gradient_ptrs[src]; });
    }

    auto aug_callee = augment(callee);

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

const Def* AutoDiffEval::wrap_cache(const App* load, const App* aug_load) {
    auto& w = world();

    auto [_, load_ptr] = load->args<2>();
    bool has_store     = has_op_store(load_ptr);

    std::string name = "cache";

    const Def* cache_ptr      = aug_load->arg(1);
    auto [load_mem, load_val] = aug_load->projs<2>();
    auto result_mem           = load_mem;

    if (has_store) {
        assert(current_loop);
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
    pb->app(true, pb->ret_var(), autodiff_zero(mem::mem_var(pb)));
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

bool is_idx(const Def* ty) {
    if (auto app = ty->isa<App>()) { return app->callee()->isa<Idx>(); }

    return false;
}

const Def* sum(const Def* left, const Def* right) {
    auto& w = left->world();
    assert(left->type() == right->type());

    if (left->isa<Bot>()) { return right; }

    if (right->isa<Bot>()) { return left; }

    if (is_idx(left->type())) {
        return core::op(core::wrap::add, core::WMode::none, left, right);
    } else if (match<core::Real>(left->type())) {
        return core::op(core::rop::add, core::RMode::none, left, right);
    } else if (left->isa<Tuple>()) {
        DefVec new_ops;
        for (size_t i = 0; i < left->num_projs(); i++) { new_ops.push_back(sum(left->proj(i), right->proj(i))); }

        return w.tuple(new_ops);
    }

    assert(false);
}

const Def* AutoDiffEval::grad_sum(const Def* def) {
    auto& w = world();
    DefVec* grads;
    if (current_loop) {
        grads = &(current_loop->attachments[def]);
    } else {
        grads = &(attachments[def]);
    }

    const Def* result = nullptr;

    for (auto grad : *grads) {
        if (result == nullptr) {
            result = grad;
        } else {
            result = sum(result, grad);
        }
    }

    return result;
}

const Def* AutoDiffEval::grad_sum(const Def* def, const Def* default_zero_ty) {
    if (auto grad = grad_arr(def)) { return grad; }

    auto result = grad_sum(def);
    if (result == nullptr) {
        auto& w = world();
        return w.lit(default_zero_ty, 0);
    }

    return result;
}

const Def* AutoDiffEval::grad_arr(const Def* def) {
    if (auto lea = match<mem::lea>(def)) {
        auto [arr, index] = lea->args<2>();
        auto grad         = grad_arr(arr);
        return mem::op_lea(grad, resolve(index));
    } else if (auto grad_ptr = gradient_ptrs[def]) {
        return grad_ptr;
    }

    return nullptr;
}

void AutoDiffEval::prop(Scope& scope, const Def* def) {
    if (!scope.bound(def)) return;
    auto& w = world();
    if (!visited_prop.insert(def).second) { return; }

    if (def->isa<App>()) {
        for (auto proj : def->projs()) { prop(scope, proj); }
    }

    auto gradient = grad_sum(def);
    if (auto load = match<mem::load>(def)) {
        auto arr  = load->arg(1);
        auto val  = load->proj(1);
        auto grad = grad_arr(arr);

        auto test = grad_sum(val);

        auto gradient_val = gradient->proj(1);
        assert(gradient_val);

        auto load_val = op_load(grad);
        auto grad_sum = sum(load_val, gradient_val);
        op_store(grad, grad_sum);
        attach_gradient(load->arg(), w.tuple({w.bot(mem::type_mem(w)), w.bot(arr->type())}));
        return;
    }

    if (gradient == nullptr) { return; }

    if (auto tuple = def->isa<Tuple>()) {
        for (size_t i = 0; i < tuple->num_ops(); i++) { attach_gradient(tuple->op(i), gradient->proj(i)); }
        return;
    }

    if (auto store = match<mem::store>(def)) {
        auto ptr      = store->arg(1);
        auto grad_ptr = grad_arr(ptr);
        auto load_val = op_load(grad_ptr);
        op_store(grad_ptr, zero(load_val->type()));
        attach_gradient(store->arg(), w.tuple({w.bot(mem::type_mem(w)), w.bot(ptr->type()), load_val}));
        return;
    }

    if (def->isa<Var>()) { return; }

    if (auto lea = match<mem::lea>(def)) { return; }

    if (auto extract = def->isa<Extract>()) {
        auto tup   = extract->tuple();
        auto index = as_lit(extract->index());

        DefArray vec(tup->num_projs(), [&](size_t i) {
            auto proj = tup->proj(i);
            if (index == i) {
                return gradient;
            } else {
                return w.bot(proj->type());
            }
        });

        attach_gradient(tup, w.tuple(vec));
        return;
    }

    if (match<mem::M>(def->type())) { return; }

    if (auto wrap = match<core::wrap>(def)) {
        auto [left, right] = wrap->args<2>();

        const Def* left_grad  = gradient;
        const Def* right_grad = gradient;

        if (wrap.id() == core::wrap::add) {
        } else if (wrap.id() == core::wrap::sub) {
            right_grad = core::op_wminus(core::WMode::none, gradient);
        } else if (wrap.id() == core::wrap::mul) {
            auto left_value  = resolve(left);
            auto right_value = resolve(right);

            left_grad  = core::op(core::wrap::mul, core::WMode::none, gradient, right_value);
            right_grad = core::op(core::wrap::mul, core::WMode::none, gradient, left_value);
        }

        attach_gradient(wrap->arg(), w.tuple({left_grad, right_grad}));
    } else if (auto div = match<core::div>(def)) {
        auto [mem, left, right] = div->args<3>();
        if (div.id() == core::div::sdiv || div.id() == core::div::udiv) {
            auto left_value  = resolve(left);
            auto right_value = resolve(right);

            auto [div_mem, left_grad] = core::op(div.id(), current_mem, gradient, right_value)->projs<2>();
            auto right_grad =
                core::op(core::wrap::mul, core::WMode::none, core::op_wminus(core::WMode::none, left_grad), left_value);
            current_mem = div_mem;
            attach_gradient(wrap->arg(), w.tuple({left_grad, right_grad}));
        }
    } else if (auto rop = match<core::rop>(def)) {
        auto [left, right] = rop->args<2>();

        const Def* left_grad  = gradient;
        const Def* right_grad = gradient;

        if (rop.id() == core::rop::add) {
        } else if (rop.id() == core::rop::sub) {
            right_grad = core::op_rminus(core::RMode::none, gradient);
        } else if (rop.id() == core::rop::mul) {
            auto left_value  = resolve(left);
            auto right_value = resolve(right);

            left_grad  = core::op(core::rop::mul, core::RMode::none, gradient, right_value);
            right_grad = core::op(core::rop::mul, core::RMode::none, gradient, left_value);
        } else if (rop.id() == core::rop::div) {
            auto left_value  = resolve(left);
            auto right_value = resolve(right);

            auto [div_mem, left_grad] = core::op(core::rop::div, current_mem, gradient, right_value)->projs<2>();
            auto right_grad =
                core::op(core::rop::mul, core::RMode::none, core::op_rminus(core::RMode::none, left_grad), left_value);
            current_mem = div_mem;
        }

        attach_gradient(rop->arg(), w.tuple({left_grad, right_grad}));
    }
}

const Def* zero_def(const Def* mem, const Def* ty) {
    auto& w = ty->world();
    if (auto sig = ty->isa<Sigma>()) {
        DefArray ops(sig->ops(), [&](const Def* op) { return zero_def(mem, op); });
        return w.tuple(ops);
    } else if (is_idx(ty)) {
        return zero(w);
    } else if (match<mem::M>(ty)) {
        return mem;
    }
}

Lam* AutoDiffEval::invert_lam(Lam* lam) {
    auto& w = world();

    w.debug_dump();

    if (lam->is_returning()) {
        auto ret_dom    = lam->ret_dom();
        auto arg_type   = lam->arg()->type();
        auto inv_arg_ty = w.sigma({ret_dom, w.cn(arg_type)});
        auto inv_pi     = w.cn(flatten_deep(inv_arg_ty));
        auto inv_lam    = w.nom_lam(inv_pi, w.dbg("inv_" + lam->name()));
        inv_lam->set_filter(true);

        inverted[lam->ret_var()] = inv_lam;

        AutodiffAnalysis analyze(lam);
        PostOrderVisitor visitor(analyze);
        Scope scope(lam);

        auto current = analyze.exit();
        current_mem  = mem::mem_var(inv_lam);

        auto current_lam = inv_lam;
        while (true) {
            assert(current);
            auto call = current->pred(Node::Type::App | Node::Type::For);
            assert(call);
            auto app = call->def->as<App>();
            auto arg = app->arg();

            auto caller     = call->pred();
            auto caller_def = caller->def;

            bool for_hack = false;
            if (call->isa(Node::Type::App)) {
                auto lam_arg_var = current_lam->arg();

                auto num_proj = arg->num_projs();
                assert(num_proj == arg->num_projs());
                attach_gradient(arg, lam_arg_var);
            } else if (call->isa(Node::Type::For)) {
                auto [start, end, inc, acc, body, exit] = arg->projs<6>();

                auto aug_start = augment(start);
                auto aug_end   = augment(end);
                auto aug_inc   = augment(inc);

                auto invert_start = zero(w);
                auto invert_inc   = one(w);

                auto size           = for_size(current_mem, aug_start, aug_end, aug_inc);
                auto invert_end     = size;
                auto invert_for_mem = end_mem();
                push_loop_frame(app, size);
                auto [invert_body, invert_exit] = invert_for_body(app);

                auto zero_acc = zero_def(invert_for_mem, invert_body->ret_dom());
                current_lam->set_body(affine::op_for(w, invert_start, invert_end, invert_inc, zero_acc->projs(),
                                                     invert_body, invert_exit));

                current_lam = invert_exit;
                for_hack    = true;
            } else {
                assert(false);
            }

            for (auto node : visitor.post_order_visit(arg)) { prop(scope, node->def); }

            DefVec inv_args;
            for (auto proj : lam->args()) {
                const Def* grad;
                if (match<mem::M>(proj->type())) {
                    grad = end_mem();
                } else {
                    grad = grad_sum(proj, proj->type());
                }
                assert(grad);
                inv_args.push_back(grad);
            }

            auto inv_arg = w.tuple(inv_args);

            if (caller_def == lam) {
                current_lam->set_body(w.app(inv_lam->ret_var(), inv_arg));
                break;
            } else {
                auto next_inv_lam = w.nom_lam(caller_def->type()->as<Pi>(), w.dbg("inv_" + lam->name()));
                next_inv_lam->set_filter(false);

                if (for_hack) { inv_arg = mem::mem_def(inv_arg); }

                current_lam->set_body(w.app(next_inv_lam, mem::mem_def(inv_arg)));
                current_lam = next_inv_lam;
                current_mem = mem::mem_var(next_inv_lam);
                current     = caller;
            }
        }

        return inv_lam;
    }

    assert(false);
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

const Def* AutoDiffEval::rewrite_rebuild(Rewriter& rewriter, const Def* def) {
    auto result = rewriter.rewrite(def);

    if (result != def) { return result; }

    if (def->isa<Var>() || def->isa<Axiom>()) { return def; }

    auto& w      = world();
    auto new_ops = DefArray(def->num_ops(), [&](auto i) { return rewrite_rebuild(rewriter, def->op(i)); });
    auto new_def = def->rebuild(w, def->type(), new_ops, def->dbg());
    return new_def;
}

std::tuple<Lam*, Lam*> AutoDiffEval::invert_for_body(const App* for_app) {
    auto& w = world();

    auto [start, end, inc, acc, body, exit] = for_app->args<6>();

    auto aug_start = augment(start);
    auto aug_inc   = augment(inc);

    auto loop_body = body->isa_nom<Lam>();
    assert(loop_body);

    auto raw_index = loop_body->arg(0_s);

    Lam* inv_loop_body = w.nom_lam(loop_body->type()->as<Pi>(), w.dbg("invert_" + loop_body->name()));
    inv_loop_body->set_filter(true);

    const Def* size_sub_one = core::op(core::wrap::sub, core::WMode::none, current_loop->local_size, one(w));

    auto normalized_index = inv_loop_body->arg(0_s);
    normalized_index      = core::op(core::wrap::sub, core::WMode::none, size_sub_one, normalized_index);

    current_loop->index()       = normalized_to_loop_index(aug_start, aug_inc, normalized_index);
    current_loop->cache_index() = normalized_to_cache_index(normalized_index);

    auto test = current_loop->index();

    add_inverted(raw_index, current_loop->index());

    auto inv_body_lam_new = invert_lam(loop_body);

    auto ret_lam = create_lam(inv_body_lam_new->ret_pi(), "invert_return_body_" + loop_body->name());

    ret_lam->set_body(w.app(inv_loop_body->ret_var(), end_mem()));

    Scope scope2(inv_body_lam_new);
    ScopeRewriter first_rewriter(world(), scope2);
    auto replacement                                = w.tuple({mem::mem_var(inv_loop_body), ret_lam});
    first_rewriter.old2new[inv_body_lam_new->var()] = replacement;
    auto new_args                                   = DefArray(inv_body_lam_new->num_ops(),
                                                               [&](size_t i) { return first_rewriter.rewrite(inv_body_lam_new->op(i)); });
    inv_loop_body->set(new_args);
    inv_body_lam_new->set_body(w.top_nat());

    // acc wrapper###############

    DefVec acc_fvs;
    DefVec acc_ops;
    Scope scope(loop_body);
    for (auto free_def : scope.free_defs()) {
        if (is_idx(free_def->type())) {
            auto acc_grad = grad_sum(free_def);
            if (acc_grad) {
                acc_ops.push_back(acc_grad);
                acc_fvs.push_back(free_def);
            }
        }
    };

    if (acc_ops.size() > 0) {
        auto acc_ty          = flatten_deep(w.sigma({mem::type_mem(w), w.tuple(acc_ops)->type()}));
        auto inv_loop_ty     = w.cn({w.type_int(32), acc_ty, w.cn(acc_ty)});
        Lam* inv_for_acc_lam = create_lam(inv_loop_ty, "invert_for_acc_" + loop_body->name());
        auto acc_index       = inv_for_acc_lam->arg(0_s);

        auto mem = end_mem();

        auto ret_acc_lam = create_lam(inv_loop_body->ret_pi(), "invert_return_acc_body_" + loop_body->name());
        auto replacement = w.tuple({acc_index, mem, ret_acc_lam});

        Scope scope(inv_loop_body);
        ScopeRewriter rewriter(world(), scope);
        rewriter.old2new[inv_loop_body->var()] = replacement;
        auto new_args =
            DefArray(inv_loop_body->num_ops(), [&](size_t i) { return rewriter.rewrite(inv_loop_body->op(i)); });
        inv_for_acc_lam->set(new_args);
        inv_loop_body->set_body(w.top_nat());

        for (auto& acc_op : acc_ops) {
            acc_op = rewrite_rebuild(first_rewriter, acc_op);
            acc_op = rewrite_rebuild(rewriter, acc_op);
        }

        auto acc_var = inv_for_acc_lam->arg(1_s);

        size_t i = 1;
        DefVec next_acc_ops;
        next_acc_ops.push_back(end_mem());
        for (auto op : acc_ops) {
            next_acc_ops.push_back(sum(op, acc_var->proj(i)));
            i++;
        }

        auto next_acc = w.tuple(next_acc_ops);
        ret_acc_lam->set_body(w.app(inv_for_acc_lam->ret_var(), next_acc));

        inv_loop_body = inv_for_acc_lam;
    }

    auto exit_for = create_lam(inv_loop_body->ret_pi(), "inv_exit_for");

    pop_loop_frame();

    size_t i = 0;
    for (; i < acc_fvs.size(); i++) { attach_gradient(acc_fvs[i], exit_for->arg(i + 1)); }

    return {inv_loop_body, exit_for};
}

const Def* AutoDiffEval::invert_for(const App* for_app) {
    auto& w = world();
    return nullptr;
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

    augmented[org_index]        = current_loop->index();
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

    // else if (auto axiom = def->isa<Axiom>()) {
    //     return axiom;
    // }

    return def;
    /*
        w.ELOG("did not expect to augment: {} : {}", def, def->type());
        w.ELOG("node: {}", def->node_name());
        assert(false && "augment not implemented on this def");*/
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

const Def* AutoDiffEval::invert_var(const Var* var) {  }
const Def* AutoDiffEval::invert_extract(const Extract* extract) {
    
}
const Def* AutoDiffEval::invert_app(const App* app) {
    
}
const Def* AutoDiffEval::invert_lit(const Lit* lit) {  }
const Def* AutoDiffEval::invert_tuple(const Tuple* tuple) { return tuple; }
const Def* AutoDiffEval::invert_pack(const Pack* pack) { return pack; }
const Def* AutoDiffEval::invert_lea(const App* lea) {
    
}

const Def* AutoDiffEval::invert_load(const App* load) { assert(false); }

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

void AutoDiffEval::attach_gradient(const Def* dst, const Def* grad) {
    /*if( match<mem::M>(dst->type()) ){
        return;
    }*/

    if (current_loop) {
        current_loop->attachments[dst].push_back(grad);
    } else {
        attachments[dst].push_back(grad);
    }
}

const Def* AutoDiffEval::invert_store(const App* store) { assert(false); }
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