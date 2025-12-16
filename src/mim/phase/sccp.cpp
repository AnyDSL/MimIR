#include "mim/phase/sccp.h"

#include <absl/container/fixed_array.h>

namespace mim {

const Def* SCCP::Analysis::join(const Def* top, const Def* def) {
    auto [i, ins] = lattice_.emplace(top, def);
    if (ins) {
        todo_ = true;
        return def;
    }

    auto cur = i->second;
    if (def->isa<Bot>() || cur == def || cur == top) return cur;

    todo_ = true;
    if (cur->isa<Bot>()) return i->second = def;
    return i->second = top;
}

Def* SCCP::Analysis::rewrite_mut(Def* mut) {
    map(mut, mut);

    if (auto var = mut->has_var()) {
        map(var, var);

        if (mut->isa<Lam>())
            for (auto var : mut->tvars()) {
                map(var, var);
                join(var, var);
            }
    }

    for (auto d : mut->deps())
        rewrite(d);

    return mut;
}

const Def* SCCP::Analysis::rewrite_imm_App(const App* app) {
    if (auto lam = app->callee()->isa_mut<Lam>(); lam && lam->is_set() && !lam->is_external() && !lam->is_annex()) {
        auto n          = app->num_targs();
        auto abstr_args = absl::FixedArray<const Def*>(n);
        auto abstr_vars = absl::FixedArray<const Def*>(n);
        for (size_t i = 0; i != n; ++i) {
            auto abstr    = rewrite(app->targ(i));
            abstr_vars[i] = join(lam->tvar(i), abstr);
            abstr_args[i] = abstr;
        }

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

bool SCCP::analyze() {
    analysis_.reset();
    analysis_.run();
    return analysis_.todo();
}

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
                    if (abstr == old_var) new_doms.emplace_back(rewrite(old_lam->dom(num_old, i)));
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
                    new_vars[i]  = abstr == old_var ? new_lam->var(num_new, j++) : rewrite(abstr);
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
                if (abstr == old_var) new_args[j++] = rewrite(old_app->targ(i));
            }

            return map(old_app, new_world().app(new_lam, new_args));
        }
    }

    return Rewriter::rewrite_imm_App(old_app);
}

} // namespace mim
