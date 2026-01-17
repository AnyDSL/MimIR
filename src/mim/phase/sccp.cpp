#include "mim/phase/sccp.h"

#include <absl/container/fixed_array.h>

namespace mim {

static nat_t get_index(const Def* def) { return Lit::as(def->as<Extract>()->index()); }

Def* SCCP::Analysis::rewrite_mut(Def* mut) {
    map(mut, mut);

    if (auto var = mut->has_var()) {
        map(var, var);

        if (mut->isa<Lam>())
            for (auto var : mut->tvars()) {
                map(var, var);
                lattice_[var] = var;
            }
    }

    for (auto d : mut->deps())
        rewrite(d);

    return mut;
}

const Def* SCCP::Analysis::propagate(const Def* top, const Def* def) {
    auto [i, ins] = lattice_.emplace(top, def);
    if (ins) {
        todo_ = true;
        return def;
    }

    auto cur = i->second;
    if (!cur || def->isa<Bot>() || cur == def || cur == top || cur->isa<Proxy>()) return cur;

    todo_ = true;
    if (cur->isa<Bot>()) return i->second = def;
    return i->second = nullptr; // we reached top for propagate; nullptr marks this to bundle for GVN
}

const Def* SCCP::Analysis::rewrite_imm_App(const App* app) {
    if (auto lam = app->callee()->isa_mut<Lam>(); isa_optimizable(lam)) {
        auto n          = app->num_targs();
        auto abstr_args = absl::FixedArray<const Def*>(n);
        auto abstr_vars = absl::FixedArray<const Def*>(n);

        // propagate
        for (size_t i = 0; i != n; ++i) {
            auto abstr    = rewrite(app->targ(i));
            abstr_vars[i] = propagate(lam->tvar(i), abstr);
            abstr_args[i] = abstr;
        }

        // GVN: bundle
        for (size_t i = 0; i != n; ++i) {
            if (abstr_vars[i]) continue;

            auto vars = DefVec();
            auto vi   = lam->tvar(i);
            auto ai   = abstr_args[i];
            vars.emplace_back(vi);

            for (size_t j = i + 1; j != n; ++j) {
                auto vj = lam->tvar(j);
                if (abstr_vars[j] || abstr_args[j] != ai) continue;
                vars.emplace_back(vj);
            }

            if (vars.size() == 1) {
                lattice_[vi] = abstr_vars[i] = vi; // top
            } else {
                auto proxy = world().proxy(vi->type(), vars, 0, 0);

                for (auto p : proxy->ops()) {
                    auto j  = get_index(p);
                    auto vi = lam->tvar(j);
                    if (abstr_vars[j] || abstr_args[j] != ai) continue;
                    lattice_[vi] = abstr_vars[j] = proxy;
                }

                DLOG("bundle: {}", proxy);
            }
        }

        // GVN: split
        for (size_t i = 0; i != n; ++i) {
            if (auto proxy = abstr_vars[i]->isa<Proxy>()) {
                auto num  = proxy->num_ops();
                auto vars = DefVec();
                auto ai   = abstr_args[i];

                for (auto p : proxy->ops()) {
                    auto j  = get_index(p);
                    auto vj = lam->tvar(j);
                    if (p == vj) {
                        if (ai == abstr_args[j]) vars.emplace_back(vj);
                    }
                }

                auto new_num = vars.size();
                if (new_num == 1) {
                    todo_        = true;
                    auto vi      = lam->tvar(i);
                    lattice_[vi] = abstr_vars[i] = vi;
                    DLOG("single: {}", vi);
                } else if (new_num != num) {
                    todo_          = true;
                    auto new_proxy = world().proxy(ai->type(), vars, 0, 0);
                    DLOG("split: {}", new_proxy);

                    for (auto p : new_proxy->ops()) {
                        auto j  = get_index(p);
                        auto vj = lam->tvar(j);
                        if (p == vj) lattice_[vj] = abstr_vars[j] = new_proxy;
                    }
                }
                // if new_num == num: do nothing
            }
        }

        // set new abstract var
        auto abstr_var = world().tuple(abstr_vars);
        map(lam->var(), abstr_var);
        lattice_[lam->var()] = abstr_var;

        if (!lookup(lam))
            for (auto d : lam->deps())
                rewrite(d);

        return world().app(lam, abstr_args);
    }

    return Rewriter::rewrite_imm_App(app);
}

static bool first_in_bundle(const Proxy* proxy, const Def* old_var) { return (proxy && proxy->op(0) == old_var); }

const Def* SCCP::rewrite_imm_App(const App* old_app) {
    if (auto old_lam = old_app->callee()->isa_mut<Lam>()) {
        if (auto l = lattice(old_lam->var()); l && l != old_lam->var()) {
            todo_ = true;

            size_t num_old = old_lam->num_tvars();
            Lam* new_lam;
            if (auto i = lam2lam_.find(old_lam); i != lam2lam_.end())
                new_lam = i->second;
            else {
                // build new dom
                auto new_doms = DefVec();
                for (size_t i = 0; i != num_old; ++i) {
                    auto old_var = old_lam->var(num_old, i);
                    auto abstr   = lattice(old_var);
                    auto proxy   = abstr->isa<Proxy>();
                    if (abstr == old_var || first_in_bundle(proxy, old_var))
                        new_doms.emplace_back(rewrite(old_lam->dom(num_old, i)));
                }

                // build new lam
                size_t num_new    = new_doms.size();
                auto new_vars     = absl::FixedArray<const Def*>(num_old);
                new_lam           = new_world().mut_lam(new_doms, rewrite(old_lam->codom()))->set(old_lam->dbg());
                lam2lam_[old_lam] = new_lam;

                // build new var
                for (size_t i = 0, j = 0; i != num_old; ++i) {
                    auto old_var = old_lam->var(num_old, i);
                    auto abstr   = lattice(old_var);
                    auto proxy   = abstr->isa<Proxy>();

                    if (abstr == old_var || first_in_bundle(proxy, old_var)) { // top or GVN bundle
                        auto v      = new_lam->var(num_new, j++);
                        new_vars[i] = v;
                        if (abstr != old_var) map(proxy, v); // GVN bundle
                    } else {
                        new_vars[i] = rewrite(abstr); // SCCP propagate
                    }
                }

                map(old_lam->var(), new_vars);
                new_lam->set(rewrite(old_lam->filter()), rewrite(old_lam->body()));
            }

            // build new app
            size_t num_new = new_lam->num_vars();
            auto new_args  = absl::FixedArray<const Def*>(num_new);
            for (size_t i = 0, j = 0; i != num_old; ++i) {
                auto old_var = old_lam->var(num_old, i);
                auto abstr   = lattice(old_var);
                auto proxy   = abstr->isa<Proxy>();
                if (abstr == old_var || (proxy && proxy->op(0) == old_var)) new_args[j++] = rewrite(old_app->targ(i));
            }

            return map(old_app, new_world().app(new_lam, new_args));
        }
    }

    return Rewriter::rewrite_imm_App(old_app);
}

} // namespace mim
