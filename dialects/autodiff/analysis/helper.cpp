#include "dialects/autodiff/analysis/helper.h"

#include "thorin/def.h"
#include "thorin/tuple.h"

#include "dialects/affine/affine.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/builder.h"
#include "dialects/math/math.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

const Def* arg(const App* app) {
    if (match<affine::For>(app)) {
        return app->arg(3);
    } else {
        return app->arg();
    }
}

const Var* is_var(const Def* def) {
    if (auto extract = def->isa<Extract>()) { return is_var(extract->tuple()); }

    return def->isa<Var>();
}

const Def* unextract(const Def* mem) {
    if (auto extract = mem->isa<Extract>()) {
        return unextract(extract->tuple());
    } else {
        return mem;
    }
}

const App* is_load_val(const Def* def) {
    if (auto extr = def->isa<Extract>()) {
        auto tuple = extr->tuple();
        if (auto load = match<mem::load>(tuple)) { return load; }
    }

    return nullptr;
}

void for_each_lam(const Def* def, std::function<void(Lam*)> f) {
    if (auto app = def->isa<App>()) {
        if (match<affine::For>(app)) {
            for_each_lam(app->arg(4), f);
            for_each_lam(app->arg(5), f);
        } else {
            for_each_lam(app->callee(), f);
        }
    } else if (auto callee_lam = def->isa_nom<Lam>()) {
        f(callee_lam);
    } else if (auto branches = is_branch(def)) {
        for (auto branch : branches->tuple()->ops()) { for_each_lam(branch, f); }
    }
}

const Def* is_idx(const Def* ty) {
    if (auto app = ty->isa<App>()) {
        if (auto idx = app->callee()->isa<Idx>()) { return app->arg(); }
    }
    return nullptr;
}

const Def* zero(const Def* ty) {
    auto& w = ty->world();
    if (auto s = math::isa_f(ty)) { return math::lit_f(w, *s, 0.0); }
    if (is_idx(ty)) { return w.lit(ty, 0); }
    if (auto arr = ty->isa<Arr>()) { return w.pack(arr->shape(), zero(arr->body())); }
    if (auto ptr = match<mem::Ptr>(ty)) { return core::op_bitcast(ty, zero(w)); }
    if (match<mem::M>(ty)) return w.bot(ty);
    assert(false);
}

const Def* one(const Def* ty) {
    auto& w = ty->world();
    if (auto s = math::isa_f(ty)) { return math::lit_f(w, *s, 1.0); }
    if (is_idx(ty)) { return w.lit(ty, 1.0); }
    if (auto arr = ty->isa<Arr>()) { return w.pack(arr->shape(), one(arr->body())); }
    assert(false);
}
const Def* zero(World& w) { return w.lit_int(32, 0); }
const Def* one(World& w) { return w.lit_int(32, 1); }

const Def* default_def(const Def* ty) {
    auto& w = ty->world();
    if (match<mem::Ptr>(ty) || match<mem::M>(ty) || ty->isa<Pi>()) return w.bot(ty);
    if (ty->isa<Sigma>()) {
        DefArray arr(ty->projs(), [](const Def* proj) { return default_def(proj); });
        return w.tuple(arr);
    } else if (auto pack = ty->isa<Arr>()) {
        return w.pack(pack->shape(), default_def(pack->body()));
    }
    return zero(ty);
}

const Def* one_hot_other_default(const Def* pattern, const Def* def, size_t position) {
    auto& w = def->world();
    DefArray vec(pattern->num_projs(), [&](size_t index) {
        auto proj = pattern->proj(index);
        if (position == index) {
            return def;
        } else {
            return default_def(proj->type());
        }
    });

    return w.tuple(vec);
}

const Extract* is_branch(const Def* callee) {
    if (auto extract = callee->isa<Extract>()) {
        if (auto tuple = extract->tuple()->isa<Tuple>()) {
            auto ops = tuple->ops();
            if (std::all_of(ops.begin(), ops.end(), [](const Def* def) { return def->type()->isa<Pi>(); })) {
                return extract;
            }
        }
    }
    return nullptr;
}

void flatten_deep(const Def* def, DefVec& ops) {
    if (def->num_projs() > 1) {
        for (auto proj : def->projs()) { flatten_deep(proj, ops); }
        return;
    }

    if (def->num_projs() == 0) { return; }

    ops.push_back(def);
}

const Def* flatten_deep(const Def* def) {
    auto& w = def->world();
    DefVec ops;
    flatten_deep(def, ops);
    if (ops[0]->sort() == Sort::Type) {
        return w.sigma(ops);
    } else {
        return w.tuple(ops);
    }
}

Lam* callee_isa_var(const Def* def) {
    if (auto proj = def->isa<Extract>()) {
        if (auto var = proj->tuple()->isa<Var>(); var && var->nom()->isa<Lam>()) { return var->nom()->as<Lam>(); }
    }
    return nullptr;
}

// R P => P'
// R TODO: function => extend
// R const Def* augment_type_fun(const Def* ty) { return ty; }
//  P => P*
//  TODO: nothing? function => R? Mem => R?
//  TODO: rename to op_tangent_type
const Def* tangent_type_fun(const Def* ty) { return ty; }

const Def* tangent_arg_type_fun(const Def* in, const Def* out) {
    auto& w = in->world();
    Builder b(w);
    b.add(in->projs());

    size_t i = 1;
    for (auto proj : out->projs()) {
        if (match<mem::Ptr>(proj)) { b.insert(i++, proj); }
    }

    return b.sigma();
}

const Def* mask(const Def* target, size_t i, const Def* def) {
    auto& w    = target->world();
    auto projs = target->projs();
    projs[i]   = def;
    return w.tuple(projs);
}

const Def* mask_last(const Def* target, const Def* def) { return mask(target, target->num_projs() - 1, def); }

const Def* merge_flat(const Def* left, const Def* right) {
    auto& w = left->world();
    return flatten_deep(w.tuple({left, right}));
}

/// computes pb type E* -> A*
/// in - type of the expression (return type for a function)
/// out - type of the argument (point of orientation resp. derivative - argument type for partial pullbacks)
const Pi* pullback_type(const Def* in, const Def* out, bool flat) {
    auto& world   = in->world();
    auto tang_arg = tangent_arg_type_fun(in, out);
    auto tang_ret = tangent_type_fun(out);

    auto dom = world.sigma({tang_arg, world.cn(tang_ret)});

    if (flat) { dom = flatten_deep(dom); }

    auto pb_ty = world.cn(dom);
    return pb_ty;
}

const Pi* forward_to_backward(const Pi* forward_pi) { return pullback_type(forward_pi->ret_dom(), forward_pi->arg()); }

// A,R => A'->R' * (R* -> A*)
const Pi* autodiff_type_fun(const Def* arg, const Def* ret, bool flat) {
    auto& w = arg->world();

    DefVec forward_in;
    for (auto def : arg->projs()) {
        forward_in.push_back(def);
        if (match<mem::Ptr>(def)) { forward_in.push_back(def); }
    }

    DefVec backward;
    for (auto def : ret->projs()) {
        if (!match<mem::Ptr>(def)) { backward.push_back(def); }
    }

    auto tangent_ret = w.cn(arg);
    backward.push_back(tangent_ret);
    auto tangent     = w.cn(backward);
    auto diff_ret_ty = w.cn(flatten_deep(w.sigma({ret, tangent})));
    forward_in.push_back(diff_ret_ty);
    auto diff_ty = w.cn(forward_in);

    return diff_ty;
}

const Pi* autodiff_type_fun_pi(const Pi* pi, bool flat) {
    auto& world = pi->world();
    if (!is_continuation_type(pi)) {
        // TODO: dependency
        auto arg = pi->dom();
        auto ret = pi->codom();
        if (ret->isa<Pi>()) {
            auto aug_arg = autodiff_type_fun(arg, flat);
            if (!aug_arg) return nullptr;
            auto aug_ret = autodiff_type_fun(pi->codom(), flat);
            if (!aug_ret) return nullptr;
            return world.pi(aug_arg, aug_ret);
        }
        return autodiff_type_fun(arg, ret, flat);
    }

    auto doms = flatten_deep(pi->dom())->projs();

    auto arg    = world.sigma(doms.skip_back());
    auto ret_pi = doms.back();

    auto ret = ret_pi->as<Pi>()->dom();
    world.DLOG("compute AD type for pi");
    auto result = autodiff_type_fun(arg, ret, flat);

    return result;
}

// P->Q => P'->Q' * (Q* -> P*)
const Def* autodiff_type_fun(const Def* ty, bool flat) {
    auto& world = ty->world();
    // TODO: handle DS (operators)
    if (auto pi = ty->isa<Pi>()) { return autodiff_type_fun_pi(pi, flat); }
    // TODO: what is this object? (only numbers are printed)
    // possible abstract type from autodiff axiom
    world.DLOG("AutoDiff on type: {}", ty);

    if (auto mem = match<mem::M>(ty)) { return mem; }

    if (auto ptr = match<mem::Ptr>(ty)) { return ptr; }

    if (auto app = ty->isa<App>()) {
        if (app->callee()->isa<Idx>()) { return ty; }
    }
    if (ty->isa<Idx>()) { return ty; }

    if (match<math::F>(ty)) { return ty; }

    if (ty == world.type_nat()) return ty;
    if (auto arr = ty->isa<Arr>()) {
        auto shape   = arr->shape();
        auto body    = arr->body();
        auto body_ad = autodiff_type_fun(body, flat);
        if (!body_ad) return nullptr;
        return world.arr(shape, body_ad);
    }
    if (auto sig = ty->isa<Sigma>()) {
        DefArray ops(sig->ops(), [&](const Def* op) { return autodiff_type_fun(op, flat); });
        return world.sigma(ops);
    }

    world.WLOG("no-diff type: {}", ty);
    return nullptr;
}

} // namespace thorin::autodiff

namespace thorin {

const Def* continuation_codom(const Def* E) {
    auto pi = E->as<Pi>();
    assert(pi != NULL);
    return pi->dom(1)->as<Pi>()->dom();
}

bool is_continuation_type(const Def* E) {
    if (auto pi = E->isa<Pi>()) { return pi->codom()->isa<Bot>(); }
    return false;
}

bool is_continuation(const Def* e) { return is_continuation_type(e->type()); }

bool is_returning_continuation(const Def* e) {
    auto E = e->type();
    if (auto pi = E->isa<Pi>()) {
        // R world.DLOG("codom is {}", pi->codom());
        // R world.DLOG("codom kind is {}", pi->codom()->node_name());
        //  duck-typing applies here
        //  use short-circuit evaluation to reuse previous results
        return is_continuation_type(pi) &&       // continuation
               pi->num_doms() == 2 &&            // args, return
               is_continuation_type(pi->dom(1)); // return type
    }
    return false;
}

const Def* continuation_dom(const Def* E) {
    auto pi = E->as<Pi>();
    assert(pi != NULL);
    if (pi->num_doms() == 0) { return pi->dom(); }
    return pi->dom(0);
}

} // namespace thorin
