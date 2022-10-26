#include "thorin/lam.h"

#include "thorin/world.h"

namespace thorin {

/*
 * Pi
 */

const Pi* Pi::ret_pi(const Def* dbg) const {
    if (num_doms() > 0) {
        auto ret = dom(num_doms() - 1, dbg);
        if (auto pi = ret->isa<Pi>(); pi != nullptr && pi->is_basicblock()) return pi;
    }

    return nullptr;
}

const Def* Pi::ret_dom(const Def* dbg) const {
    auto ret_pi_def = ret_pi();
    if (!ret_pi_def) return nullptr;
    return ret_pi_def->dom();
}

Pi* Pi::set_dom(Defs doms) { return Def::set(0, world().sigma(doms))->as<Pi>(); }
bool Pi::is_cn() const { return codom()->isa<Bot>(); }

const Def* Pi::arg() const {
    if (is_returning()) {
        auto dom_arr = doms();
        return world().sigma(dom_arr.skip_back());
    } else {
        return dom();
    }
}

/*
 * Lam
 */

const Def* Lam::ret_var(const Def* dbg) { return type()->ret_pi() ? var(num_vars() - 1, dbg) : nullptr; }

const Def* Lam::arg(const Def* dbg) {
    if (is_returning()) {
        auto var_arr = vars();
        return world().tuple(var_arr.skip_back(), dbg);
    } else {
        return var(dbg);
    }
}

Lam* Lam::set_filter(Filter filter) {
    const Def* f;
    if (auto b = std::get_if<bool>(&filter))
        f = world().lit_bool(*b);
    else
        f = std::get<const Def*>(filter);
    return Def::set(0, f)->as<Lam>();
}

Lam* Lam::app(Filter f, const Def* callee, const Def* arg, const Def* dbg) {
    assert(isa_nom() && !filter());
    set_filter(f);
    return set_body(world().app(callee, arg, dbg));
}

Lam* Lam::app(Filter filter, const Def* callee, Defs args, const Def* dbg) {
    return app(filter, callee, world().tuple(args), dbg);
}

Lam* Lam::branch(Filter filter, const Def* cond, const Def* t, const Def* f, const Def* mem, const Def* dbg) {
    return app(filter, world().select(t, f, cond), mem, dbg);
}

Lam* Lam::test(Filter filter,
               const Def* value,
               const Def* index,
               const Def* match,
               const Def* clash,
               const Def* mem,
               const Def* dbg) {
    return app(filter, world().test(value, index, match, clash), mem, dbg);
}

/*
 * Pi
 */

// TODO remove
Lam* get_var_lam(const Def* def) {
    if (auto extract = def->isa<Extract>()) return extract->tuple()->as<Var>()->nom()->as<Lam>();
    return def->as<Var>()->nom()->as<Lam>();
}

} // namespace thorin
