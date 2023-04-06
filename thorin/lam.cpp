#include "thorin/lam.h"

#include "thorin/world.h"

namespace thorin {

/*
 * Pi
 */

const Pi* Pi::ret_pi() const {
    if (num_doms() > 0) {
        auto ret = dom(num_doms() - 1);
        if (auto pi = ret->isa<Pi>(); pi != nullptr && pi->is_basicblock()) return pi;
    }

    return nullptr;
}

Pi* Pi::set_dom(Defs doms) { return Def::set(0, world().sigma(doms))->as<Pi>(); }
bool Pi::is_cn() const { return codom()->isa<Bot>(); }

/*
 * Lam
 */

const Def* Lam::ret_var() { return type()->ret_pi() ? var(num_vars() - 1) : nullptr; }

Lam* Lam::set_filter(Filter filter) {
    const Def* f;
    if (auto b = std::get_if<bool>(&filter))
        f = world().lit_bool(*b);
    else
        f = std::get<const Def*>(filter);
    return Def::set(0, f)->as<Lam>();
}

Lam* Lam::app(Filter f, const Def* callee, const Def* arg) {
    assert(isa_mut() && !filter());
    set_filter(f);
    return set_body(world().app(callee, arg));
}

Lam* Lam::app(Filter filter, const Def* callee, Defs args) { return app(filter, callee, world().tuple(args)); }

Lam* Lam::branch(Filter filter, const Def* cond, const Def* t, const Def* f, const Def* mem) {
    return app(filter, world().select(t, f, cond), mem);
}

Lam* Lam::test(Filter filter, const Def* value, const Def* index, const Def* match, const Def* clash, const Def* mem) {
    return app(filter, world().test(value, index, match, clash), mem);
}

std::deque<const App*> decurry(const Def* def) {
    std::deque<const App*> apps;
    while (auto app = def->isa<App>()) {
        apps.emplace_front(app);
        def = app->callee();
    }
    return apps;
}

} // namespace thorin
