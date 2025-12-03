#include "mim/plug/mem/phase/ssa_phase.h"

#include <absl/container/fixed_array.h>

namespace mim::plug::mem::phase {

bool SSAPhase::analyze() {
    int i = 0;
    while (todo_) {
        ILOG("iter: {}", i++);
        todo_ = false;
        visited_.clear();
        for (auto def : old_world().annexes())
            concr2abstr(init(def));
        for (auto def : old_world().externals().muts())
            concr2abstr(init(def));
    }

    return false; // no fixed-point neccessary
}

const Def* SSAPhase::init(const Def* def) {
    if (auto mut = def->isa_mut()) return init(mut);
    return def;
}

const Def* SSAPhase::init(Def* mut) {
    if (auto var = mut->has_var()) concr2abstr(var, var);
    return mut;
}

const Def* SSAPhase::concr2abstr(const Def* concr) {
    if (auto [_, ins] = visited_.emplace(concr); !ins) {
        if (auto i = concr2abstr_.find(concr); i != concr2abstr_.end()) return i->second;
        // in some rare cyclic cases we haven't build the immutabe yet
    }

    if (auto mut = concr->isa_mut()) {
        concr2abstr(mut, mut);
        init(mut);
        for (auto d : mut->deps())
            concr2abstr(d);
        return mut;
    }

    return concr2abstr(concr, concr2abstr_impl(concr)).first;
}

const Def* SSAPhase::concr2abstr_impl(const Def* def) {
    if (auto type = def->type()) concr2abstr(type);

    if (auto branch = Branch(def)) {
        auto abstr = concr2abstr(branch.cond());
        auto l     = Lit::isa<bool>(abstr);
        if (l && *l) return concr2abstr(branch.tt());
        if (l && !*l) return concr2abstr(branch.ff());
    } else if (auto app = def->isa<App>()) {
        if (auto lam = app->callee()->isa_mut<Lam>(); lam && lam->is_set()) {
            auto ins = concr2abstr(lam, lam).second;

            auto n          = app->num_args();
            auto abstr_args = absl::FixedArray<const Def*>(n);
            auto abstr_vars = absl::FixedArray<const Def*>(n);
            for (size_t i = 0; i != n; ++i) {
                auto abstr    = concr2abstr(app->targ(i));
                abstr_vars[i] = concr2abstr(lam->tvar(i), abstr).first;
                abstr_args[i] = abstr;
            }

            concr2abstr(lam->var(), old_world().tuple(abstr_vars));

            if (ins)
                for (auto d : lam->deps())
                    concr2abstr(d);

            return old_world().app(lam, abstr_args);
        }
    } else if (auto var = def->isa<Var>()) {
        assert(var->mut()->isa<Lam>());
        return old_world().bot(var->type());
    }

    auto n      = def->num_ops();
    auto abstrs = absl::FixedArray<const Def*>(n);
    for (size_t i = 0; i != n; ++i)
        abstrs[i] = concr2abstr(def->op(i));

    return def->rebuild(old_world(), def->type(), abstrs);
}

std::pair<const Def*, bool> SSAPhase::concr2abstr(const Def* concr, const Def* abstr) {
    auto [_, v_ins]   = visited_.emplace(concr);
    auto [i, c2a_ins] = concr2abstr_.emplace(concr, abstr);

    if (c2a_ins)
        todo_ = true;
    else
        i->second = join(concr, i->second, abstr);

    return {i->second, v_ins};
}

const Def* SSAPhase::join(const Def* concr, const Def* abstr1, const Def* abstr2) {
    const Def* result = nullptr;
    if (abstr1 == abstr2) return abstr1;

    if (abstr1->isa<Bot>())
        result = abstr2;
    else if (abstr2->isa<Bot>())
        result = abstr1;
    else // abstr1 != abstr2)
        result = concr;

    todo_ |= abstr1 != result;
    return result;
}

const Def* SSAPhase::rewrite_imm_App(const App* old_app) {
    if (auto old_lam = old_app->callee()->isa_mut<Lam>(); old_lam && old_lam->is_set() && !old_lam->is_external()) {
        if (old_lam->has_var()) {
            size_t num_old = old_lam->num_vars();

            Lam* new_lam;
            if (auto i = lam2lam_.find(old_lam); i != lam2lam_.end())
                new_lam = i->second;
            else {
                // build new dom
                auto new_doms = DefVec();
                for (size_t i = 0; i != num_old; ++i) {
                    auto old_var = old_lam->var(num_old, i);
                    auto abstr   = concr2abstr_[old_var];
                    if (abstr == old_var) new_doms.emplace_back(rewrite(old_lam->tdom(i)));
                }

                // build new lam
                size_t num_new    = new_doms.size();
                auto new_vars     = absl::FixedArray<const Def*>(num_old);
                new_lam           = new_world().mut_lam(new_doms, rewrite(old_lam->codom()))->set(old_lam->dbg());
                lam2lam_[old_lam] = new_lam;

                // build new var
                for (size_t i = 0, j = 0; i != num_old; ++i) {
                    auto old_var = old_lam->var(num_old, i);
                    auto abstr   = concr2abstr_[old_var];
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
                auto abstr   = concr2abstr_[old_var];
                if (abstr == old_var) new_args[j++] = rewrite(old_app->targ(i));
            }

            return map(old_app, new_world().app(new_lam, new_args));
        }
    }

    return Rewriter::rewrite_imm_App(old_app);
}

} // namespace mim::plug::mem::phase
