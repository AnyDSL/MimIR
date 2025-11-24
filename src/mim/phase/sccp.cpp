#include "mim/phase/sccp.h"

#include "absl/container/fixed_array.h"
#include "fe/assert.h"

namespace mim {

bool SCCP::analyze() {
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

const Def* SCCP::init(const Def* def) {
    if (auto mut = def->isa_mut()) return init(mut);
    return def;
}

const Def* SCCP::init(Def* mut) {
    if (auto var = mut->has_var()) concr2abstr(var, var);
    return mut;
}

const Def* SCCP::concr2abstr(const Def* concr) {
    if (auto [_, inserted] = visited_.emplace(concr); inserted) {
        if (auto mut = concr->isa_mut()) {
            // concr2abstr_[mut] = mut;
            concr2abstr(mut, mut);
            init(mut);
            return concr2abstr_impl(concr);
        }

        auto abstr = concr2abstr_impl(concr);
        // concr2abstr_[concr] = abstr;
        concr2abstr(concr, abstr);
        return abstr;
    }

    auto i = concr2abstr_.find(concr);
    assert(i != concr2abstr_.end());
    return i->second;
}

const Def* SCCP::concr2abstr_impl(const Def* def) {
    if (auto type = def->type()) concr2abstr(type);

    if (auto branch = Branch(def)) {
        auto abstr = branch.cond();
        auto l = Lit::isa<bool>(abstr);
        if (l && *l) concr2abstr(branch.tt());
        if (l && !*l) concr2abstr(branch.ff());
    } else if (auto app = def->isa<App>()) {
        if (auto lam = app->callee()->isa_mut<Lam>()) {
            auto ins = concr2abstr(lam, lam).second;

            auto n          = app->num_args();
            auto abstr_args = absl::FixedArray<const Def*>(n);
            auto abstr_vars = absl::FixedArray<const Def*>(n);
            for (size_t i = 0; i != n; ++i) {
                auto abstr    = concr2abstr(app->arg(n, i));
                abstr_args[i] = abstr;
                abstr_vars[i] = concr2abstr(lam->var(n, i), abstr).first;
            }

            concr2abstr(lam->var(), old_world().tuple(abstr_vars));

            if (ins)
                for (auto d : lam->deps())
                    concr2abstr(d);

            return old_world().app(lam, abstr_args);
        }
    } else if (auto mut = def->isa_mut()) {
        for (auto d : mut->deps())
            concr2abstr(d);
        return mut;
    } else if (auto lam = def->isa_mut<Lam>()) {
        init(lam);
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

std::pair<const Def*, bool> SCCP::concr2abstr(const Def* concr, const Def* abstr) {
    auto [_, v_ins]   = visited_.emplace(concr);
    auto [i, c2a_ins] = concr2abstr_.emplace(concr, abstr);

    if (c2a_ins)
        todo_ = true;
    else
        i->second = join(concr, i->second, abstr);

    return {i->second, v_ins};
}

const Def* SCCP::join(const Def* concr, const Def* abstr1, const Def* abstr2) {
    const Def* result = nullptr;
    if (abstr1 == abstr2) return abstr1;

    if (abstr1->isa<Bot>())
        result = abstr2;
    else if (abstr2->isa<Bot>())
        result = abstr1;
    else // abstr1 != abstr2)
        result = concr;

    todo_ |= abstr1 != result;
    concr->dump();
    abstr1->dump();
    abstr2->dump();
    result->dump();
    std::cout << "--" << std::endl;
    return result;
}

#if 0
const Def* SCCP::join(const Def* var, const Def* abstr) {
    auto abstr_var = concr2abstr(var);

    const Def* result = nullptr;

    if (abstr_var->isa<Bot>())
        result = abstr;
    else if (abstr_var == abstr)
        result = abstr_var;
    else
        result = var;

    if (result != abstr_var) {
        result->dump();
        todo_ = true;
    }

    return concr2abstr(var, result), result;
}
#endif

const Def* SCCP::rewrite_imm_App(const App* old_app) {
    if (auto old_lam = old_app->callee()->isa_mut<Lam>(); old_lam && old_lam->is_set()) {
        if (auto var = old_lam->has_var()) {
            size_t num_old = old_lam->num_vars();

            Lam* new_lam;
            if (auto i = lam2lam_.find(old_lam); i != lam2lam_.end())
                new_lam = i->second;
            else {
                // build new dom
                auto new_doms = DefVec();
                for (size_t i = 0; i != num_old; ++i) {
                    auto old_var = var->proj(num_old, i);
                    auto abstr   = concr2abstr_[var->proj(num_old, i)];
                    if (abstr == old_var) new_doms.emplace_back(rewrite(old_lam->dom(num_old, i)));
                }

                // build new lam
                size_t num_new    = new_doms.size();
                auto new_vars     = absl::FixedArray<const Def*>(num_old);
                new_lam           = new_world().mut_lam(new_doms, rewrite(old_lam->codom()))->set(old_lam->dbg());
                lam2lam_[old_lam] = new_lam;

                for (size_t i = 0, j = 0; i != num_old; ++i) {
                    auto old_var = var->proj(num_old, i);
                    auto abstr   = concr2abstr_[old_var];
                    if (abstr == old_var)
                        new_vars[i] = new_lam->var(num_new, j++);
                    else
                        new_vars[i] = rewrite(abstr);
                }
                auto tup = new_world().tuple(new_vars);
                map(var, tup);

                // TODO or below?
                new_lam->set(rewrite(old_lam->filter()), rewrite(old_lam->body()));
            }

            // build new app
            size_t num_new = new_lam->num_vars();
            auto new_args  = absl::FixedArray<const Def*>(num_new);
            for (size_t i = 0, j = 0; i != num_old; ++i) {
                auto old_var = var->proj(num_old, i);
                auto abstr   = concr2abstr_[old_var];
                if (abstr == old_var) new_args[j++] = rewrite(old_app->arg(num_old, i));
            }

            auto new_app = new_world().app(new_lam, new_args);
            map(old_app, new_app);
            return new_app;
        }
        // }
    }

    return Rewriter::rewrite_imm_App(old_app);
}

} // namespace mim
