
#include "thorin/pass/fp/unbox_closures.h"
#include "thorin/transform/closure_conv.h"

namespace thorin {

const Def* UnboxClosure::rewrite(const Def* def) { 
    auto& w = world();

    if (auto proj = def->isa<Extract>(); proj && isa_ctype(proj->type())) {
        auto tuple = proj->tuple()->isa<Tuple>();
        if (!tuple || tuple->num_ops() <= 0)
            return def;
        DefVec envs, lams;
        const Def* fnc_type = nullptr;
        for (auto op: tuple->ops()) {
            auto c = isa_closure_lit(op);
            if (c.mark() != CA::bot) {
                w.DLOG("brach {}: wait for closure destruct", def);
                return def;
            }
            // Note: We have to check if the pi's and not just the environmen-types are *equal*, since
            // extract doesn't check for equiv and the closure conv may rewrite noms with different, but equiv noms
            if (!c || !c.fnc_as_lam() || c.is_proc() || c.is(CA::unknown) || (fnc_type && fnc_type != c.fnc_type()))
                return def;
            fnc_type = c.fnc_type();
            envs.push_back(c.env());
            lams.push_back(c.fnc_as_lam());
        }
        auto env = w.extract(w.tuple(envs), proj->index());
        auto fnc = w.extract(w.tuple(lams), proj->index());
        auto new_def = pack_closure(env, fnc, proj->type());
        w.DLOG("flattend branch: {} => {}", tuple, new_def);
        return new_def;
    }

    if (auto app = def->isa<App>()) {
        auto bxd_lam = app->callee()->isa_nom<Lam>();
        if (ignore(bxd_lam) || keep_.contains(bxd_lam))
            return def;
        auto& arg_spec = data(bxd_lam);
        DefVec doms, args, proxy_ops = {bxd_lam};
        for (size_t i = 0; i < app->num_args(); i++) {
            auto arg = app->arg(i);
            auto type = arg->type();
            if (!isa_ctype(type) || keep_.contains(bxd_lam->var(i))) {
                doms.push_back(type);
                args.push_back(arg);
                continue;
            }
            auto c = isa_closure_lit(arg, false);
            if (!c) {
                w.DLOG("{}({}) => ⊤ (no closure lit)" , bxd_lam, i);
                keep_.emplace(bxd_lam->var(i));
                proxy_ops.push_back(arg);
                continue;
            }
            if (arg_spec[i] && arg_spec[i] != c.env_type()) {
                w.DLOG("{}({}): {} => ⊤  (env mismatch: {})" , bxd_lam, i, arg_spec[i], c.env_type());
                keep_.emplace(bxd_lam->var(i));
                proxy_ops.push_back(arg);
                continue;
            }
            if (!arg_spec[i]) {
                arg_spec[i] = c.env_type();
                w.DLOG("{}({}): ⊥ => {}", bxd_lam, i, c.env_type());
            }
            doms.push_back(w.sigma({c.fnc_type(), c.env_type()}));
            args.push_back(w.tuple({c.fnc(), c.env()}));
        }

        if (proxy_ops.size() > 1) {
            return proxy(def->type(), proxy_ops);
        }

        // No argument was flattend, ignore lam from now on
        if (doms.size() <= bxd_lam->num_doms()) {
            w.DLOG("KEEP {}", bxd_lam);
            keep_.emplace(bxd_lam);
            return def;
        }

        auto& [ubxd_lam, old_doms] = boxed2unboxed_[bxd_lam];
        if (!ubxd_lam || old_doms != doms) {
            old_doms = doms;
            ubxd_lam = bxd_lam->stub(w, w.cn(w.sigma(doms)), bxd_lam->dbg());
            ubxd_lam->set_name(bxd_lam->name());
            auto new_vars = w.tuple(DefArray(bxd_lam->num_doms(), [&](auto i) {
                auto old_var = bxd_lam->var(i);
                auto new_var = ubxd_lam->var(i);
                if (auto ct = isa_ctype(old_var->type()); ct && !keep_.contains(old_var))
                    return pack_closure(new_var->proj(1), new_var->proj(0), ct);
                return new_var;
            }));
            ubxd_lam->set(bxd_lam->apply(new_vars));
            w.DLOG("{} => {} (new)", bxd_lam, ubxd_lam);
            keep_.insert(ubxd_lam);
        } else {
            w.DLOG("{} => {} (cached)", bxd_lam, ubxd_lam);
        }
        return w.app(ubxd_lam, args);
    }
    return def;
}

undo_t UnboxClosure::analyze(const Proxy* def) {
    auto lam = def->op(0_u64)->isa_nom<Lam>();
    assert(lam);
    world().DLOG("undo {}", lam);
    return undo_visit(lam);
}

};

