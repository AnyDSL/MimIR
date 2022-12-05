#pragma once

#include <deque>

#include "thorin/def.h"

namespace thorin {

class Checker {
public:
    Checker(World& world)
        : world_(&world) {}

    World& world() const { return *world_; }

    /// Are @p d1 and @p d2 α-equivalent?
    /// If both @p d1 and @p d2 are Def::unset Infer%s, @p opt%imisitc indicates the return value.
    /// Usually, you want to be **opt**imisitic; Checker::is_uniform, however, is pessimistic.
    bool equiv(Refer d1, Refer d2, Refer dbg, bool opt = true);

    /// Can @p value be assigned to sth of @p type?
    /// @note This is different from `equiv(type, value->type(), dbg)` since @p type may be dependent.
    bool assignable(Refer type, Refer value, Refer dbg);

    /// Yields `defs.front()`, if all @p defs are α-equiv%alent and `nullptr` otherwise.
    const Def* is_uniform(Defs defs, Refer dbg);

    static void swap(Checker& c1, Checker& c2) { std::swap(c1.world_, c2.world_); }

private:
    bool equiv_internal(Refer, Refer, Refer, bool);

    enum class Equiv {
        Distinct,
        Unknown,
        Equiv,
    };

    World* world_;
    DefDefMap<Equiv> equiv_;
    std::deque<std::pair<Def*, Def*>> vars_;
};

} // namespace thorin
