#pragma once

#include <deque>

#include "thorin/def.h"

namespace thorin {

const Def* infer_type_level(World&, Defs);

class Checker {
public:
    Checker(World& world)
        : world_(&world) {}

    World& world() const { return *world_; }

    /// Are @p d1 and @p d2 alpha-equivalent?
    bool equiv(const Def* d1, const Def* d2, const Def* dbg);

    /// Can @p value be assigned to sth of @p type?
    /// @note This is different from `equiv(type, value->type(), dbg)` since @p type may be dependent.
    bool assignable(const Def* type, const Def* value, const Def* dbg);

    /// Yields `defs.front()`, if all @p defs are alpha-equiv%alent and `nullptr` otherwise.
    const Def* is_uniform(Defs defs, const Def* dbg);

    static void swap(Checker& c1, Checker& c2) { std::swap(c1.world_, c2.world_); }

private:
    bool equiv_internal(const Def*, const Def*, const Def*);

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
