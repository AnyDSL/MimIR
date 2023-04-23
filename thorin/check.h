#pragma once

#include <deque>

#include "thorin/def.h"

namespace thorin {

/// This node is a hole in the IR that is inferred by its context later on.
/// It is modelled as a *mut*able Def.
/// If inference was successful, it's Infer::op will be set to the inferred Def.
class Infer : public Def {
private:
    Infer(const Def* type)
        : Def(Node, type, 1, 0) {}

public:
    /// @name op
    ///@{
    const Def* op() const { return Def::op(0); }
    Infer* set(const Def* op) { return Def::set(0, op)->as<Infer>(); }
    Infer* unset() { return Def::unset()->as<Infer>(); }
    ///@}

    /// @name union-find
    ///@{
    /// [Union-Find](https://en.wikipedia.org/wiki/Disjoint-set_data_structure) to unify Infer nodes.
    /// Def::flags is used to keep track of rank for
    /// [Union by rank](https://en.wikipedia.org/wiki/Disjoint-set_data_structure#Union_by_rank).
    static const Def* find(const Def*);
    ///@}

    Infer* stub(World&, Ref) override;

private:
    flags_t rank() const { return flags(); }
    flags_t& rank() { return flags_; }

    THORIN_DEF_MIXIN(Infer)
    friend class Checker;
};

class Checker {
public:
    Checker(World& world)
        : world_(&world) {}

    World& world() const { return *world_; }

    /// Are @p d1 and @p d2 α-equivalent?
    bool equiv(Ref d1, Ref d2);

    /// Can @p value be assigned to sth of @p type?
    /// @note This is different from `equiv(type, value->type())` since @p type may be dependent.
    bool assignable(Ref type, Ref value);

    /// Yields `defs.front()`, if all @p defs are α-equiv%alent and `nullptr` otherwise.
    const Def* is_uniform(Defs defs);

    static void swap(Checker& c1, Checker& c2) { std::swap(c1.world_, c2.world_); }

private:
    bool equiv_internal(Ref, Ref);

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
