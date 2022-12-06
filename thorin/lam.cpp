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

Pi* Pi::set_dom(Defs doms) { return Def::set(0, world().sigma(doms))->as<Pi>(); }
bool Pi::is_cn() const { return codom()->isa<Bot>(); }

/*
 * Lam
 */

const Def* Lam::ret_var(const Def* dbg) { return type()->ret_pi() ? var(num_vars() - 1, dbg) : nullptr; }

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
 * implicits
 */

const Def* implicits2meta(World& world, const std::vector<bool>& implicits) {
    const Def* meta = world.bot(world.type_bool());
    for (auto b : implicits | std::ranges::views::reverse)
        meta = world.tuple({world.lit_bool(b), meta});
    return meta;
}

std::optional<std::pair<bool, const Def*>> peel_implicit(const Def* def) {
    if (def) {
        if (auto tuple = def->isa<Tuple>(); tuple && tuple->num_ops() == 2) {
            if (auto b = isa_lit<bool>(tuple->op(0))) {
                outln("{} - {}", *b, tuple->op(1));
                return {{*b, tuple->op(1)}};
            }
        }
    }
    return {};
}

} // namespace thorin
