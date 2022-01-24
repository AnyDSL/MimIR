#include "thorin/lam.h"

#include "thorin/world.h"

namespace thorin {

/*
 * Pi
 */

const Pi* Pi::ret_pi(const Def* dbg) const {
    if (num_doms() > 0) {
        auto ret = dom(num_doms() - 1, dbg);
        if (auto pi = ret->isa<Pi>(); pi != nullptr && pi->is_cn()) return pi;
    }

    return nullptr;
}

Pi* Pi::set_dom(Defs doms) { return Def::set(0, world().sigma(doms))->as<Pi>(); }

bool Pi::is_cn() const { return codom()->isa<Bot>(); }

/*
 * Lam
 */

Lam* Lam::set_filter(bool filter) { return set_filter(world().lit_bool(filter)); }

const Def* Lam::mem_var(const Def* dbg) {
    return thorin::isa<Tag::Mem>(var(0_s)->type()) ? var(0, dbg) : nullptr;
}

const Def* Lam::ret_var(const Def* dbg) {
    return type()->ret_pi() ? var(num_vars() - 1, dbg) : nullptr;
}

bool Lam::is_basicblock() const { return type()->is_basicblock(); }

void Lam::app(const Def* callee, const Def* arg, const Def* dbg) {
    assert(isa_nom());
    auto filter = world().lit_false();
    set(filter, world().app(callee, arg, dbg));
}

void Lam::app(const Def* callee, Defs args, const Def* dbg) { app(callee, world().tuple(args), dbg); }

void Lam::branch(const Def* cond, const Def* t, const Def* f, const Def* mem, const Def* dbg) {
    return app(world().select(t, f, cond), mem, dbg);
}

void Lam::test(const Def* value, const Def* index, const Def* match, const Def* clash, const Def* mem, const Def* dbg) {
    return app(world().test(value, index, match, clash), mem, dbg);
}

/*
 * Pi
 */

// TODO remove
Lam* get_var_lam(const Def* def) {
    if (auto extract = def->isa<Extract>())
        return extract->tuple()->as<Var>()->nom()->as<Lam>();
    return def->as<Var>()->nom()->as<Lam>();
}

}
