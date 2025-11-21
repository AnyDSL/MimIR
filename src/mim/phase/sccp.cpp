#include "mim/phase/sccp.h"

namespace mim {

bool SCCP::analyze() {
    while (todo_) {
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
    if (auto lam = def->isa_mut<Lam>()) {
        if (auto var = lam->has_var()) concr2abstr_[var] = var;
    }
    return def;
}

const Def* SCCP::concr2abstr(const Def* def) {
    if (auto [_, inserted] = visited_.emplace(def); !inserted) {
        auto i = concr2abstr_.find(def);
        assert(i != concr2abstr_.end());
        return i->second;
    }

    return concr2abstr_[def] = concr2abstr_impl(def);
}

const Def* SCCP::concr2abstr_impl(const Def* def) {
    if (auto type = def->type()) concr2abstr(type);

    if (auto app = def->isa<App>()) {
        if (auto lam = app->callee()->isa_mut<Lam>()) {
            auto n          = app->num_args();
            auto abstr_args = absl::FixedArray<const Def*>(n);
            auto abstr_vars = absl::FixedArray<const Def*>(n);
            for (size_t i = 0; i != n; ++i) {
                auto abstr    = concr2abstr(app->arg(i));
                abstr_args[i] = abstr;
                abstr_vars[i] = join(lam->var(i), abstr);
            }

            concr2abstr_[lam->var()] = old_world().tuple(abstr_vars);

            for (auto d : lam->deps())
                concr2abstr(d);

            return old_world().app(lam, abstr_args);
        }
    }

    if (auto mut = def->isa_mut()) {
        concr2abstr_[mut] = mut;
        if (auto lam = mut->isa<Lam>()) init(lam);

        for (auto d : mut->deps())
            concr2abstr(d);

        return mut;
    }

    if (auto var = def->isa<Var>()) {
        if (var->mut()->isa<Lam>()) return old_world().bot(var->type());
        return var;
    }

    if (auto lam = def->isa_mut<Lam>()) init(lam);
    auto n      = def->num_ops();
    auto abstrs = absl::FixedArray<const Def*>(n);
    for (size_t i = 0; i != n; ++i)
        abstrs[i] = concr2abstr(def->op(i));

    return def->rebuild(old_world(), def->type(), abstrs);
}

const Def* SCCP::join(const Def* var, const Def* abstr) {
    auto abstr_var = concr2abstr(var);

    const Def* result;

    if (abstr_var->isa<Bot>()) result = abstr;
    else if (abstr_var == abstr) result = abstr_var;
    else result = var;

    if (result != abstr_var) todo_ = true;

    return result;
}

#if 0
const Def* SCCP::rewrite_imm_App(const App* app) {
    if (auto old_lam = app->callee()->isa_mut<Lam>(); old_lam && old_lam->is_set() && is_candidate(old_lam)) {
        DLOG("beta-reduce: `{}`", old_lam);
        if (auto var = old_lam->has_var()) {
            auto new_arg = rewrite(app->arg());
            map(var, new_arg);
            // if we want to reduce more than once, we need to push/pop
        }
        todo_ = true;
        return rewrite(old_lam->body());
    }

    return Rewriter::rewrite_imm_App(app);
}
#endif

} // namespace mim
