
#include "thorin/pass/fp/unbox_closures.h"
#include "thorin/transform/closure_conv.h"

namespace thorin {

static std::tuple<std::vector<ClosureLit>, const Def*> isa_branch(const Def* callee) {
    if (auto closure_proj = callee->isa<Extract>()) {
        auto inner_proj = closure_proj->tuple()->isa<Extract>();
        if (inner_proj && inner_proj->tuple()->isa<Tuple>() && isa_ctype(inner_proj->type())) {
            auto branches = std::vector<ClosureLit>();
            for (auto op: inner_proj->tuple()->ops()) {
                if (auto c = isa_closure_lit(op))
                    branches.push_back(std::move(c));
                else
                    return {};
            }
            return {branches, inner_proj->index()};
        }
    }
    return {};
}

const Def* UnboxClosure::rewrite(const Def* def) { 
    auto& w = world();
    auto app = def->isa<App>();
    if (!app) return def;

    if (auto [branches, index] = isa_branch(app->callee()); index) {
        w.DLOG("FLATTEN BRANCH {}", app->callee());
        auto new_branches = w.tuple(DefArray(branches.size(), [&](auto i) {
            auto c = branches[i];
            auto [entry, inserted] = branch2dropped_.emplace(c, nullptr);
            auto& dropped_lam = entry->second;
            if (inserted || !dropped_lam) {
                auto clam = c.fnc_as_lam();
                dropped_lam = clam->stub(w, ctype_to_pi(c.type(), w.sigma()), clam->dbg());
                auto new_vars = DefArray(dropped_lam->num_doms(), [&](auto i) {
                    return (i == CLOSURE_ENV_PARAM) ? c.env() : dropped_lam->var(i); 
                });
                dropped_lam->set(clam->apply(w.tuple(new_vars)));
            }
            return dropped_lam;
        }));
        return w.app(w.extract(new_branches, index), closure_remove_env(app->arg()));
    }

    // auto bxd_lam = app->callee()->isa_nom<Lam>();
    // if (ignore(bxd_lam) || keep_.contains(bxd_lam)) return def;

    // auto& arg_spec = data(bxd_lam);
    // auto num_unboxed = 0;
    // DefVec doms, args, proxy_ops = {bxd_lam};
    // for (size_t i = 0; i < app->num_args(); i++) {
    //     auto arg = app->arg(i);
    //     auto type = arg->type();
    //     if (keep_.contains(bxd_lam->var(i))) {
    //         doms.push_back(type);
    //         args.push_back(arg);
    //         continue;
    //     }
    //     if (!isa_ctype(type)) {
    //         doms.push_back(type);
    //         args.push_back(arg);
    //         keep_.emplace(bxd_lam->var(i));
    //         continue;
    //     }
    //     auto c = isa_closure_lit(arg, false);
    //     if (!c || c.is_basicblock()) {
    //         w.DLOG("{}({}) => ⊤ (no returning closure lit)" , bxd_lam, i);
    //         keep_.emplace(bxd_lam->var(i));
    //         proxy_ops.push_back(arg);
    //         continue;
    //     }
    //     if (arg_spec[i] && arg_spec[i] != c.env_type()) {
    //         w.DLOG("{}({}): {} => ⊤  (env mismatch: {})" , bxd_lam, i, arg_spec[i], c.env_type());
    //         keep_.emplace(bxd_lam->var(i));
    //         proxy_ops.push_back(arg);
    //         continue;
    //     }
    //     if (!arg_spec[i]) {
    //         arg_spec[i] = c.env_type();
    //         w.DLOG("{}({}): ⊥ => {}", bxd_lam, i, c.env_type());
    //     }
    //     num_unboxed++;
    //     doms.push_back(w.sigma({c.fnc_type(), c.env_type()}));
    //     args.push_back(w.tuple({c.fnc(), c.env()}));
    // }

    // if (proxy_ops.size() > 1)
    //     return proxy(def->type(), proxy_ops);
    // if (num_unboxed == 0)
    //     return def;

    // auto& [ubxd_lam, old_doms] = boxed2unboxed_[bxd_lam];
    // if (!ubxd_lam || old_doms != doms) {
    //     old_doms = doms;
    //     ubxd_lam = bxd_lam->stub(w, w.cn(w.sigma(doms)), bxd_lam->dbg());
    //     ubxd_lam->set_name(bxd_lam->name());
    //     auto new_vars = w.tuple(DefArray(bxd_lam->num_doms(), [&](auto i) {
    //         auto old_var = bxd_lam->var(i);
    //         auto new_var = ubxd_lam->var(i);
    //         if (auto ct = isa_ctype(old_var->type()); ct && !keep_.contains(old_var))
    //             return pack_closure(new_var->proj(1), new_var->proj(0), ct);
    //         return new_var;
    //     }));
    //     ubxd_lam->set(bxd_lam->apply(new_vars));
    //     w.DLOG("{} => {} (new)", bxd_lam, ubxd_lam);
    //     keep_.insert(ubxd_lam);
    // } else {
    //     w.DLOG("{} => {} (cached)", bxd_lam, ubxd_lam);
    // }

    // return w.app(ubxd_lam, args);
    return def;
}

undo_t UnboxClosure::analyze(const Proxy* def) {
    auto& w = world();
    auto lam = def->op(0_u64)->as_nom<Lam>();
    w.DLOG("undo {}", lam);
    auto vars = lam->vars();
    if (std::all_of(vars.begin(), vars.end(), [&](auto v) { return keep_.contains(v); })) {
        w.DLOG("keep {}", lam);
        keep_.emplace(lam);
    }
    return undo_visit(lam);
}

};

