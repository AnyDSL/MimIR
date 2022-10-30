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

bool is_idx(const Def* ty) {
    if (auto app = ty->isa<App>()) { return app->callee()->isa<Idx>(); }
    return false;
}

const Def* zero(const Def* ty) {
    auto& w = ty->world();
    if (match<core::Real>(ty)) { return w.lit(ty, 0.0); }
    if (is_idx(ty)) { return w.lit(ty, 0); }
    if (auto arr = ty->isa<Arr>()) { return w.pack(arr->shape(), zero(arr->body())); }
    return w.lit(ty, 0);
}
const Def* zero(World& w) { return w.lit_int(32, 0); }
const Def* one(World& w) { return w.lit_int(32, 1); }

Lam* AutoDiffEval::create_block(const std::string& name) {
    auto& w  = world();
    auto lam = build(w).mem_ty().mem_cn().with_filter().lam(name);
    init_mem(lam);
    return lam;
}

Lam* AutoDiffEval::create_ret(const Lam* lam) {
    auto& w      = world();
    auto ret_lam = w.nom_lam(lam->type()->as<Pi>()->ret_pi(), w.dbg("return_" + lam->name()));
    ret_lam->set_filter(true);
    init_mem(ret_lam);
    return ret_lam;
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

    push_mem(aug_lam);
    auto aug_body = augment(lam->body());
    aug_lam->set_body(aug_body);
    pop_mem();
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

const Def* AutoDiffEval::augment_app(const App* app) {
    auto& world = app->world();

    auto callee = app->callee();
    auto arg    = app->arg();

    auto aug_arg = augment(arg);

    auto aug_callee = augment(callee);

    if (mem::mem_def(aug_arg)) { aug_arg = mem::replace_mem(end_mem(), aug_arg); }

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

const Def* AutoDiffEval::create_init_alloc_frame(const std::string& name, const Def* alloc_ty, const Def* init) {
    const Def* ptr = nullptr;
    create_init_frame("alloc_" + name, [&](const Def* mem) {
        auto& w                                 = mem->world();
        auto alloc_cache                        = mem::op_malloc(alloc_ty, mem, w.dbg(name));
        auto [alloc_cache_mem, alloc_cache_ptr] = alloc_cache->projs<2>();
        ptr                                     = alloc_cache_ptr;
        if (init) { alloc_cache_mem = mem::op_store(alloc_cache_mem, alloc_cache_ptr, init); }
        return alloc_cache_mem;
    });

    return ptr;
}

void AutoDiffEval::preserve(const Def* target) {
    if (requires_caching.contains(target)) {
        requires_caching.erase(target);

        const Def* value = augment(target);
        auto& w          = world();

        auto loop_size   = current_loop->size;
        auto cache_index = current_loop->cache_index();

        auto loop_size_nat = core::op_bitcast(w.type_nat(), loop_size);

        auto arr_ty = w.arr(loop_size_nat, value->type());

        auto alloc_cache_ptr = create_init_alloc_frame("cache_" + value->name(), arr_ty);

        auto unsized_arr_ptr_ty = mem::type_ptr(w.arr(w.top(w.type_nat()), value->type()));
        const Def* cache_ptr    = core::op_bitcast(unsized_arr_ptr_ty, alloc_cache_ptr, alloc_cache_ptr->dbg());

        auto cache_lea = mem::op_lea(cache_ptr, cache_index, w.dbg("lea_cache"));
        op_store(cache_lea, value, w.dbg("store_cache"));
        cache_map[target]                = cache_ptr;
        cache_loop_assignment[cache_ptr] = current_loop;
    }
}

const Def* AutoDiffEval::augment_load(const App* load) {
    auto& w = world();

    auto [_, aug_ptr] = augment(load->arg())->projs<2>();

    auto aug_load = op_load_mem(aug_ptr, w.dbg(aug_ptr->name() + "_val"));
    return aug_load;
}

const Def* AutoDiffEval::augment_store(const App* store) {
    auto& world = store->world();

    auto aug_arg               = augment(store->arg());
    auto [_, aug_ptr, aug_val] = aug_arg->projs<3>();
    auto aug_store             = op_store(aug_ptr, aug_val, store->dbg());
    return aug_store;
}

const Def* AutoDiffEval::augment_slot(const App* slot) {
    auto& world = slot->world();
    augment(slot->arg(0_s));
    auto type     = slot->decurry()->arg(0_s);
    auto aug_slot = op_slot_mem(type);
    auto grad_ptr = op_slot(type);
    op_store(grad_ptr, zero(type));
    gradient_pointers[slot->proj(1)] = grad_ptr;
    return aug_slot;
}

const Def* AutoDiffEval::augment_alloc(const App* alloc) {
    auto org_ptr = alloc->proj(1);
    augment(alloc->arg(0_s));

    auto type          = alloc->decurry()->arg(0_s);
    auto aug_alloc_mem = op_alloc_mem(type);
    auto gradient_ptr  = create_init_alloc_frame("gradient_arr", type, zero(type));

    // add to list of allocated memory
    allocated_memory.insert(gradient_ptr);
    gradient_pointers[org_ptr] = gradient_ptr;

    return aug_alloc_mem;
}

const Def* AutoDiffEval::augment_malloc(const App* malloc) {
    auto aug_arg = augment(malloc->arg());
    // TODO: not yet implemented
    return malloc;
}

const Def* AutoDiffEval::augment_bitcast(const App* bitcast) {
    auto& world = bitcast->world();
    auto arg    = bitcast->arg();

    auto aug_arg = augment(arg);
    auto dst     = core::op_bitcast(bitcast->type(), aug_arg, arg->dbg());

    if (auto grad_ptr = gradient_pointers[arg]) {
        gradient_pointers[bitcast] = core::op_bitcast(bitcast->type(), grad_ptr, arg->dbg());
    }

    return dst;
}

const Def* for_size(/*const Def*& mem,*/ const Def* start, const Def* end, const Def* inc) {
    auto& w            = start->world();
    auto span          = core::op(core::wrap::sub, core::WMode::none, end, start);
    auto inc_minus_one = core::op(core::wrap::sub, core::WMode::none, inc, one(w));
    auto size          = core::op(core::wrap::add, core::WMode::none, span, inc_minus_one);
    // auto [mem2, size]  = core::op(core::div::udiv, mem, span_ext, inc)->projs<2>();
    // mem                = mem2;
    return size;
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

const Def* AutoDiffEval::get_gradient(const Def* def) { return current_loop->gradient[def]; }

const Def* AutoDiffEval::get_gradient(const Def* def, const Def* default_zero_ty) {
    if (auto grad = grad_arr(def)) { return grad; }

    auto result = get_gradient(def);
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
        assert(grad);
        return mem::op_lea(grad, resolve(index));
    } else if (auto grad_ptr = gradient_pointers[def]) {
        return grad_ptr;
    }

    return nullptr;
}

const Def* one_hot_other_bot(const Def* pattern, const Def* def, size_t position) {
    auto& w = def->world();
    DefArray vec(pattern->num_projs(), [&](size_t index) {
        auto proj = pattern->proj(index);
        if (position == index) {
            return def;
        } else {
            return w.bot(proj->type());
        }
    });

    return w.tuple(vec);
}

const Def* scale(const Def* shape, const Def* def) {
    auto& w = shape->world();
    if (def->num_projs() > 1) {
        DefArray new_ops(def->projs(), [&](const Def* op) { return scale(shape, op); });
        return w.tuple(new_ops);
    }

    if (match<core::Real>(def->type())) {
        auto shape_idx       = core::op_bitcast(w.type_int(64), shape);
        auto shape_val       = core::op(core::conv::u2r, def->type(), shape_idx);
        auto scaled_gradient = core::op(core::rop::mul, core::RMode::none, shape_val, def);
        return scaled_gradient;
    } else if (is_idx(def->type())) {
        auto shape_idx       = core::op_bitcast(def->type(), shape);
        auto scaled_gradient = core::op(core::wrap::mul, core::WMode::none, shape_idx, def);
        return scaled_gradient;
    }

    assert(false);
}

void AutoDiffEval::prop(Scope& scope, const Def* def) {
    if (!scope.bound(def)) return;
    if (def->isa<Var>()) { return; }
    if (!visited_prop.insert(def).second) { return; }

    if (def->isa<App>()) {
        for (auto proj : def->projs()) { prop(scope, proj); }
    }

    auto& w       = world();
    auto gradient = get_gradient(def);
    if (gradient == nullptr) { return; }

    if (auto load = match<mem::load>(def)) {
        auto arr  = load->arg(1);
        auto val  = load->proj(1);
        auto grad = grad_arr(arr);

        assert(grad);
        auto test = get_gradient(val);

        auto gradient_val = gradient->proj(1);
        assert(gradient_val);

        auto load_val     = op_load(grad);
        auto get_gradient = sum(load_val, gradient_val);
        op_store(grad, get_gradient);
        return;
    }

    if (auto tuple = def->isa<Tuple>()) {
        auto ops  = tuple->ops();
        auto ops2 = gradient->ops();
        for (size_t i = 0; i < tuple->num_ops(); i++) { attach_gradient(tuple->op(i), gradient->proj(i)); }
        return;
    }

    if (auto store = match<mem::store>(def)) {
        auto ptr      = store->arg(1);
        auto grad_ptr = grad_arr(ptr);
        auto load_val = op_load(grad_ptr);
        op_store(grad_ptr, zero(load_val->type()));
        attach_gradient(store->arg(), one_hot_other_bot(store->arg(), load_val, 2));
        return;
    }

    if (auto extract = def->isa<Extract>()) {
        auto tup   = extract->tuple();
        auto index = as_lit(extract->index());

        attach_gradient(tup, one_hot_other_bot(tup, gradient, index));
        return;
    }

    if (auto pack = def->isa<Pack>()) {
        auto shape = resolve(pack->shape());
        auto body  = pack->body();

        auto gradient_val = gradient->as<Pack>()->body();
        attach_gradient(body, scale(shape, gradient_val));
        return;
    }

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
            right->dump(1);
            auto left_value  = resolve(left);
            auto right_value = resolve(right);

            left_grad  = core::op(core::rop::mul, core::RMode::none, gradient, right_value);
            right_grad = core::op(core::rop::mul, core::RMode::none, gradient, left_value);
        } else if (rop.id() == core::rop::div) {
            auto left_value  = resolve(left);
            auto right_value = resolve(right);

            left_grad = core::op(core::rop::div, core::RMode::none, gradient, right_value);
            right_grad =
                core::op(core::rop::mul, core::RMode::none, core::op_rminus(core::RMode::none, left_grad), left_value);
        }

        attach_gradient(rop->arg(), w.tuple({left_grad, right_grad}));
    }
}

const Def* zero_def(const Def* mem, const Def* ty) {
    auto& w = ty->world();
    if (auto sig = ty->isa<Sigma>()) {
        DefArray ops(sig->ops(), [&](const Def* op) { return zero_def(mem, op); });
        return w.tuple(ops);
    } else if (match<mem::M>(ty)) {
        return mem;
    } else {
        return zero(ty);
    }
}

Lam* AutoDiffEval::invert_lam(Lam* lam) {
    auto& w = world();

    if (lam->is_returning()) {
        auto ret_dom    = lam->ret_dom();
        auto arg_type   = lam->arg()->type();
        auto inv_arg_ty = w.sigma({ret_dom, w.cn(arg_type)});
        auto inv_pi     = w.cn(flatten_deep(inv_arg_ty));
        auto inv_lam    = create_lam(inv_pi, "inv_" + lam->name());

        AutodiffAnalysis analyze(lam);
        PostOrderVisitor visitor(analyze);
        Scope scope(lam);

        auto current     = analyze.exit();
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

                auto size       = for_size(/*current_mem,*/ aug_start, aug_end, aug_inc);
                auto invert_end = size;

                auto body_lam = app->arg(4)->as_nom<Lam>();

                Scope scope(body_lam);
                for (const Def* free_def : scope.free_defs()) {
                    auto free_ty = free_def->type();
                    if (is_idx(free_ty) || match<core::Real>(free_ty)) { resolve(free_def); }
                }

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
                    grad = get_gradient(proj, proj->type());
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
                init_mem(next_inv_lam);
                current = caller;
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

    add_inverted(raw_index, current_loop->index());

    auto inv_body_lam_new = invert_lam(loop_body);

    auto ret_lam = create_lam(inv_body_lam_new->ret_pi(), "invert_return_body_" + loop_body->name());

    ret_lam->set_body(w.app(inv_loop_body->ret_var(), end_mem()));

    Scope scope2(inv_body_lam_new);
    ScopeRewriter first_rewriter(world(), scope2);
    auto replacement = w.tuple({mem::mem_var(inv_loop_body), ret_lam});
    first_rewriter.map(inv_body_lam_new->var(), replacement);
    auto new_args = DefArray(inv_body_lam_new->num_ops(),
                             [&](size_t i) { return first_rewriter.rewrite(inv_body_lam_new->op(i)); });
    inv_loop_body->set(new_args);
    inv_body_lam_new->set_body(w.top_nat());

    // acc wrapper###############

    DefVec acc_fvs;
    DefVec acc_ops;
    Scope scope(loop_body);
    for (auto free_def : scope.free_defs()) {
        auto free_ty = free_def->type();
        if (is_idx(free_ty) || match<core::Real>(free_ty)) {
            auto acc_grad = get_gradient(free_def);
            if (acc_grad) {
                acc_ops.push_back(acc_grad);
                acc_fvs.push_back(free_def);
            }
        }
    };

    if (!acc_ops.empty()) {
        auto acc_ty          = flatten_deep(w.sigma({mem::type_mem(w), w.tuple(acc_ops)->type()}));
        auto inv_loop_ty     = w.cn({w.type_int(32), acc_ty, w.cn(acc_ty)});
        Lam* inv_for_acc_lam = create_lam(inv_loop_ty, "invert_for_acc_" + loop_body->name());
        auto acc_index       = inv_for_acc_lam->arg(0_s);

        auto mem = end_mem();

        auto ret_acc_lam = create_lam(inv_loop_body->ret_pi(), "invert_return_acc_body_" + loop_body->name());
        auto replacement = w.tuple({acc_index, mem, ret_acc_lam});

        Scope scope(inv_loop_body);
        ScopeRewriter rewriter(world(), scope);
        rewriter.map(inv_loop_body->var(), replacement);
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
    if (auto cached_loop = loop_assignment[for_app]) {
        current_loop = cached_loop;
    } else {
        auto next_frame          = std::make_shared<LoopFrame>(*this);
        next_frame->parent       = current_loop;
        next_frame->size         = core::op(core::wrap::mul, core::WMode::none, current_loop->size, size);
        next_frame->local_size   = size;
        loop_assignment[for_app] = next_frame;
        current_loop             = next_frame;
    }
}

void AutoDiffEval::pop_loop_frame() {
    assert(current_loop->parent);
    current_loop = current_loop->parent;
}

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

    push_mem(aug_body);
    aug_body->set_body(augment(loop_body->body()));
    pop_mem();

    return aug_body;
}

const Def* AutoDiffEval::augment_for(const App* for_app) {
    auto& w                                 = world();
    auto [start, end, inc, acc, body, exit] = for_app->args<6>();

    auto aug_start = augment(start);
    auto aug_end   = augment(end);
    auto aug_inc   = augment(inc);
    auto aug_acc   = augment(acc);

    auto size = for_size(aug_start, aug_end, aug_inc);

    auto body_lam = body->as_nom<Lam>();

    Scope scope(body_lam);
    for (const Def* free_def : scope.free_defs()) {
        auto free_ty = free_def->type();
        if (is_idx(free_ty) || match<core::Real>(free_ty)) { augment(free_def); }
    }

    push_loop_frame(for_app, size);
    auto aug_body = augment_for_body(body, aug_start, aug_inc);
    pop_loop_frame();
    auto aug_exit = augment(exit);

    auto mem = end_mem();

    if (current_loop == root) {
        Lam* current   = build(w).mem_ty().lam("init_caches");
        InitFrame last = {.lam = current, .mem = mem::mem_var(current)};
        for (; !init_frames.empty();) {
            auto next = init_frames.top();
            init_frames.pop();
            last.lam->set_body(w.app(next.lam, last.mem));
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

    w.DLOG("Augment def {} : {}", def, def->type());

    if (auto for_app = match<affine::For>(def)) { return augment_for(for_app); }

    if (auto lea = match<mem::lea>(def)) { return augment_lea(lea->as<App>()); }

    if (auto load = match<mem::load>(def)) { return augment_load(load->as<App>()); }

    if (auto store = match<mem::store>(def)) { return augment_store(store->as<App>()); }

    if (auto slot = match<mem::slot>(def)) { return augment_slot(slot->as<App>()); }

    if (auto malloc = match<mem::malloc>(def)) { return augment_malloc(malloc->as<App>()); }

    if (auto alloc = match<mem::alloc>(def)) { return augment_alloc(alloc->as<App>()); }

    if (auto bitcast = match<core::bitcast>(def)) { return augment_bitcast(bitcast->as<App>()); }

    // app => cont, operator, function
    if (auto app = def->isa<App>()) {
        return augment_app(app);
    }

    // projection
    else if (auto ext = def->isa<Extract>()) {
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

    return def;
}

const Def* AutoDiffEval::invert_(const Def* def) {
    auto& w = world();

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

const Def* AutoDiffEval::invert_var(const Var* var) {}
const Def* AutoDiffEval::invert_extract(const Extract* extract) {}
const Def* AutoDiffEval::invert_app(const App* app) {}
const Def* AutoDiffEval::invert_lit(const Lit* lit) {}
const Def* AutoDiffEval::invert_tuple(const Tuple* tuple) { return tuple; }
const Def* AutoDiffEval::invert_pack(const Pack* pack) { return pack; }
const Def* AutoDiffEval::invert_lea(const App* lea) {}

const Def* AutoDiffEval::invert_load(const App* load) { assert(false); }

const Def* AutoDiffEval::op_load_mem(const Def* ptr, const Def* dbg) {
    check_mem();
    auto load   = mem::op_load(current_mem, ptr, dbg);
    current_mem = load->proj(0);
    return load;
}

const Def* AutoDiffEval::op_load(const Def* ptr, const Def* dbg) { return op_load_mem(ptr, dbg)->proj(1); }

const Def* AutoDiffEval::op_store(const Def* ptr, const Def* value, const Def* dbg) {
    check_mem();
    current_mem = mem::op_store(current_mem, ptr, value, dbg);
    return current_mem;
}

const Def* AutoDiffEval::op_free(const Def* ptr, const Def* dbg) {
    check_mem();
    current_mem = mem::op_free(current_mem, ptr, dbg);
    return current_mem;
}

const Def* AutoDiffEval::op_slot_mem(const Def* type, const Def* dbg) {
    check_mem();
    auto& w     = world();
    auto slot   = mem::op_slot(type, current_mem, dbg);
    current_mem = slot->proj(0);
    return slot;
}

const Def* AutoDiffEval::op_slot(const Def* type, const Def* dbg) { return op_slot_mem(type, dbg)->proj(1); }

const Def* AutoDiffEval::op_alloc_mem(const Def* type, const Def* dbg) {
    check_mem();
    auto& w     = world();
    auto slot   = mem::op_alloc(type, current_mem, dbg);
    current_mem = slot->proj(0);
    return slot;
}

const Def* AutoDiffEval::op_alloc(const Def* type, const Def* dbg) { return op_alloc_mem(type, dbg)->proj(1); }

const Def* AutoDiffEval::resolve(const Def* def) {
    auto& w = world();

    assert(!match<mem::M>(def->type()));
    if (def->isa<Axiom>()) { return def; }
    if (auto inv_def = inverted[def]) { return inv_def; }
    assert(!def->isa<Var>());

    if (auto cache_arr = cache_map[def]) {
        auto loop        = cache_loop_assignment[cache_arr];
        auto cache_index = loop->cache_index();
        auto cache_lea   = mem::op_lea(cache_arr, cache_index);
        return op_load(cache_lea);
    }

    if (auto load = match<mem::load>(def)) {
        auto ptr       = load->arg(1);
        auto cache_ptr = resolve(ptr);
        return op_load_mem(cache_ptr);
    }

    auto new_ops  = DefArray(def->num_ops(), [&](auto i) { return resolve(def->op(i)); });
    auto new_def  = def->rebuild(w, def->type(), new_ops, def->dbg());
    inverted[def] = new_def;
    return new_def;
}

void AutoDiffEval::attach_gradient(const Def* dst, const Def* new_gradient) {
    auto gradient = current_loop->gradient[dst];

    if (gradient) {
        gradient = sum(gradient, new_gradient);
    } else {
        gradient = new_gradient;
    }

    current_loop->gradient[dst] = gradient;
}

const Def* AutoDiffEval::invert_store(const App* store) {}

const Def* AutoDiffEval::invert_malloc(const App* malloc) {}

const Def* AutoDiffEval::invert_alloc(const App* alloc) {}

const Def* AutoDiffEval::invert_bitcast(const App* bitcast) {}

} // namespace thorin::autodiff