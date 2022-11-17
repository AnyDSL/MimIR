
#include "dialects/mem/passes/rw/reshape.h"

#include <functional>

#include "thorin/check.h"

#include "dialects/mem/mem.h"

namespace thorin::mem {

void Reshape::enter() {
    rewrite(curr_nom());

    while (!worklist_.empty()) {
        auto lam = worklist_.top();
        worklist_.pop();
        if (lam->is_set()) {
            auto app           = lam->body()->as<App>();
            auto [callee, arg] = app->ops<2>();
            lam->set(callee->reduce(arg));
        }
    }
}

const Def* Reshape::rewrite(const Def* def) {
    if (auto i = old2new_.find(def); i != old2new_.end()) return i->second;
    auto new_def      = rewrite_convert(def);
    old2new_[def]     = new_def;
    old2new_[new_def] = new_def;
    return new_def;
}

const Def* reshape(const Def* arg, const Pi* target_pi);

void Reshape::aggregate_sigma(const Def* ty, DefQueue& ops) {
    if (auto sigma = ty->isa<Sigma>()) {
        for (auto op : sigma->ops()) { aggregate_sigma(op, ops); }
        return;
    }

    auto new_ty = convert_ty(ty);
    if (match<mem::M>(new_ty)) {
        ops.push_front(new_ty);
    } else {
        ops.push_back(new_ty);
    }

    return;
}

const Def* Reshape::flatten_ty_convert(const Def* ty) {
    auto& w = ty->world();
    if (auto arr = ty->isa<Arr>()) {
        auto new_body = flatten_ty(arr->body());
        auto shape    = arr->shape();
        return w.arr(shape, new_body);
    } else if (auto cn = ty->isa<Pi>()) {
        auto dom   = flatten_ty(cn->dom());
        auto codom = flatten_ty(cn->codom());
        return w.pi(dom, codom);
    } else if (auto sigma = ty->isa<Sigma>()) {
        DefQueue ops;
        aggregate_sigma(sigma, ops);
        return w.sigma(DefArray(ops.begin(), ops.end()));
    }

    return ty;
}
const Def* Reshape::flatten_ty(const Def* ty) {
    auto flatten_ty = old2flatten_[ty];
    if (flatten_ty) return flatten_ty;
    auto flat_ty     = flatten_ty_convert(ty);
    old2flatten_[ty] = flat_ty;
    return flat_ty;
}

const Def* Reshape::convert_ty(const Def* ty) {
    auto& w = ty->world();

    if (mode_ == Mode::Arg) {
        if (auto arr = ty->isa<Arr>()) {
            auto new_body = convert_ty(arr->body());
            auto shape    = arr->shape();
            return w.arr(shape, new_body);
        } else if (ty->isa<Pi>()) {
            auto test = flatten_ty(ty)->as<Pi>();
            auto doms = test->doms();

            if (doms.back()->isa<Pi>()) {
                auto skip = doms.skip_back();
                return w.cn({w.sigma(skip), doms.back()});
            }

            return w.cn({w.sigma(doms)});
        }
    } else if (mode_ == Mode::Flat) {
        ty = flatten_ty(ty);
    } else {
        assert(false);
    }

    return ty;
}

void lam_vars(const Def* def, std::function<void(const Def*)> collector) {
    if (def->num_projs() > 1) {
        for (auto proj : def->projs()) { lam_vars(proj, collector); }

        return;
    }

    collector(def);
}

void convert(const Def* def, DefVec& vars) {
    lam_vars(def, [&](const Def* def) { vars.push_back(def); });
}

void convert(const Def* def, DefQueue& vars) {
    lam_vars(def, [&](const Def* def) {
        if (match<mem::M>(def->type())) {
            vars.push_front(def);
        } else {
            vars.push_back(def);
        }
    });
}

const Def* Reshape::wrap(const Def* def, const Def* target_ty) {
    auto def_ty    = def->type()->isa<Pi>();
    auto target_pi = target_ty->isa<Pi>();
    if (!def_ty || !target_pi) return def;
    if (target_pi == def_ty) return def;

    auto& w      = def->world();
    auto wrapper = w.nom_lam(target_pi, def->dbg());
    auto arg     = reshape(wrapper->var(), def_ty);

    wrapper->set_body(w.app(def, arg));
    wrapper->set_filter(true);
    if (target_pi == convert_ty(target_pi)) { worklist_.push(wrapper); }
    return wrapper;
}

const Def* Reshape::convert(const Def* def) {
    auto pi_ty = def->type()->isa<Pi>();
    if (!pi_ty) return def;
    auto new_ty = convert_ty(pi_ty);
    if (new_ty == pi_ty) return def;
    return wrap(def, new_ty)->as_nom<Lam>();
}

const Def* Reshape::reshape(const Def* mem, const Def* ty, DefQueue& vars) {
    if (ty->isa<Sigma>() || ty->isa<Arr>()) {
        auto& w = ty->world();
        // op is not number of components in general
        // auto ops = DefArray(ty->num_ops(), [&](auto i) { return reshape(mem, ty->op(i), vars); });
        auto ops = DefArray(ty->num_projs(), [&](auto i) { return reshape(mem, ty->proj(i), vars); });
        return w.tuple(ty, ops);
    }

    if (match<mem::M>(ty)) { return mem; }

    assert(vars.size() != 0);
    const Def* next = vars.front();
    vars.pop_front();

    auto& w = ty->world();
    w.DLOG("Reshape next: {} : {}", next, next->type());

    return wrap(next, ty);
    ;
}

const Def* fill_extract_mem(const Def* def, DefQueue& vars) {
    const Def* mem = nullptr;
    lam_vars(def, [&](const Def* def) {
        if (match<mem::M>(def->type())) {
            mem = def;
        } else if (!(def->type()->isa<Sigma>() && def->type()->num_ops() == 0)) {
            // ignore [] elements
            vars.push_back(def);
        }
    });
    return mem;
}

const Def* Reshape::reshape(const Def* arg, const Pi* target_pi) {
    DefQueue queue;
    auto& w = arg->world();
    w.DLOG("Reshape::reshape: arg = {} : {}", arg, arg->type());
    w.DLOG("Reshape::reshape: target_pi = {} : {}", target_pi, target_pi->type());
    w.DLOG("Reshape::reshape: target_dom = {} : {}", target_pi->dom(), target_pi->dom()->type());
    const Def* mem  = fill_extract_mem(arg, queue);
    auto target_arg = reshape(mem, target_pi->dom(), queue);
    w.DLOG("Reshape::reshape: target_arg = {} : {}", target_arg, target_arg->type());
    // w.DLOG("Reshape::reshape: queue = {,}", queue);
    w.DLOG("Queue size: {}", queue.size());
    for (auto var : queue) { w.DLOG("  Reshape::reshape: var = {} : {}", var, var->type()); }
    assert(queue.empty());
    return target_arg;
}

// TODO: should be handled more generally
const Def* Reshape::rewrite_convert(const Def* def) {
    switch (def->node()) {
        case Node::Bot:
        case Node::Top:
        case Node::Type:
        case Node::Univ:
        case Node::Nat: return def;
    }

    if (def->isa<Var>() || def->isa<Axiom>()) { return def; }

    auto& w       = world();
    auto new_type = rewrite(def->type());
    auto new_dbg  = def->dbg() ? rewrite(def->dbg()) : nullptr;

    // if (auto malloc = match<mem::malloc>(def)) {
    //     auto [mem, size] = rewrite(malloc->arg())->projs<2>();
    //     auto type        = malloc->decurry()->arg(0);

    //     auto new_type = convert_ty(type);

    //     return mem::op_malloc(new_type, mem, malloc->dbg());
    // } else if (auto lea = match<mem::lea>(def)) {
    //     auto [ptr, idx] = rewrite(lea->arg())->projs<2>();

    //     return mem::op_lea(ptr, idx);
    // } else if (auto store = match<mem::store>(def)) {
    //     auto [mem, ptr, val] = rewrite(store->arg())->projs<3>();
    //     auto new_val         = convert(val);

    //     return mem::op_store(mem, ptr, new_val);
    // } else if (auto load = match<mem::load>(def)) {
    //     auto [rewritten_mem, rewritten_ptr] = rewrite(load->arg())->projs<2>();
    //     auto ptr_ty                         = match<mem::Ptr>(load->arg(1)->type())->arg(0);
    //     auto [mem, val]                     = mem::op_load(rewritten_mem, rewritten_ptr)->projs<2>();
    //     auto new_val                        = wrap(val, ptr_ty);

    //     return w.tuple({mem, new_val});
    // } else
    if (auto app = def->isa<App>()) {
        auto callee = rewrite(app->op(0));
        auto arg    = rewrite(app->op(1));

        if (!callee->isa<Axiom>()) { arg = reshape(arg, callee->type()->as<Pi>()); }

        auto new_app = w.app(callee, arg);
        return new_app;
    } else if (auto lam = def->isa_nom<Lam>()) {
        Lam* new_lam = lam;
        if (lam->is_set()) { new_lam = convert(lam)->as_nom<Lam>(); }

        old2new_[def] = new_lam;

        if (lam->is_set()) {
            auto new_body = rewrite(lam->body());
            lam->set_body(new_body);
        }

        return new_lam;
    } else {
        auto new_ops = DefArray(def->num_ops(), [&](auto i) { return rewrite(def->op(i)); });

        auto new_def = def->rebuild(w, new_type, new_ops, new_dbg);

        return new_def;
    }
}

} // namespace thorin::mem
