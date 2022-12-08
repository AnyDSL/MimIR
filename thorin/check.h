#pragma once

#include <deque>

#include "thorin/def.h"

namespace thorin {

/// This node is a hole in the IR that is inferred by its context later on.
/// It is modelled as a *nom*inal Def.
/// If inference was successful,
class Infer : public Def {
private:
    Infer(const Def* type, const Def* dbg)
        : Def(Node, type, 1, 0, dbg) {}

public:
    /// @name op
    ///@{
    const Def* op() const { return Def::op(0); }
    Infer* set(const Def* op) { return Def::set(0, op)->as<Infer>(); }
    ///@}

    const Def* inflate(Refer type, Defs elems_t);
    const Def* inflate(Refer type, u64 n, Refer elem_t);

    /// @name virtual methods
    ///@{
    Infer* stub(World&, const Def*, const Def*) override;
    ///@}

    static constexpr auto Node = Node::Infer;
    friend class World;
};

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
