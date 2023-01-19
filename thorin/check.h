#pragma once

#include <deque>

#include "thorin/def.h"

namespace thorin {

/// This node is a hole in the IR that is inferred by its context later on.
/// It is modelled as a *nom*inal Def.
/// If inference was successful, it's Infer::op will be set to the inferred Def.
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

    /// @name inflate
    ///@{
    /// If we figure out that an Infer is a Tuple/Sigma, we create a new Tuple/Sigma where each element is an Infer.
    const Def* inflate(Ref type, Defs elems_t);
    const Def* inflate(Ref type, u64 n, Ref elem_t);
    ///@}

    /// @name virtual methods
    ///@{
    Infer* stub(World&, const Def*, const Def*) override;
    ///@}

    /// @name union-find
    ///@{
    /// [Union-Find](https://en.wikipedia.org/wiki/Disjoint-set_data_structure) to unify Infer nodes.
    /// Def::flags is used to keep track of rank for
    /// [Union by rank](https://en.wikipedia.org/wiki/Disjoint-set_data_structure#Union_by_rank).
    static const Def* find(const Def*);
    static const Def* unite(Ref, Ref);
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
    bool equiv(Ref d1, Ref d2, Ref dbg);

    /// Can @p value be assigned to sth of @p type?
    /// @note This is different from `equiv(type, value->type(), dbg)` since @p type may be dependent.
    bool assignable(Ref type, Ref value, Ref dbg);

    /// Yields `defs.front()`, if all @p defs are α-equiv%alent and `nullptr` otherwise.
    const Def* is_uniform(Defs defs, Ref dbg);

    static void swap(Checker& c1, Checker& c2) { std::swap(c1.world_, c2.world_); }

private:
    bool equiv_internal(Ref, Ref, Ref);

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
