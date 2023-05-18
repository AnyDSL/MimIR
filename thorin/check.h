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
    Infer* reset(const Def* op) { return Def::reset(0, op)->as<Infer>(); }
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

/// Result of Checker::equiv.
enum class Equiv {
    No, ///< Definitely **not** α-equivalent.
    FV, ///< α-equivalent **modulo free variables** such as `λx.x+y` and `λx.x+z`.
    Yes,///< **Definitely** α-equivalent.
};

inline Equiv meet(Equiv e1, Equiv e2) { return (Equiv)std::min((int)e1, (int)e2); }
inline Equiv join(Equiv e1, Equiv e2) { return (Equiv)std::max((int)e1, (int)e2); }

class Checker {
public:
    Checker(World& world)
        : world_(&world) {}

    World& world() const { return *world_; }

    /// Are @p d1 and @p d2 α-equivalent? @see Equiv.
    Equiv equiv_(Ref d1, Ref d2);

    /// Can @p value be assigned to sth of @p type?
    /// @note This is different from `equiv(type, value->type())` since @p type may be dependent.
    Equiv assignable_(Ref type, Ref value);

    /// Yields `defs.front()`, if all @p defs are Equiv::Yes and `nullptr` otherwise.
    const Def* is_uniform(Defs defs);

private:
    Equiv equiv_internal(Ref, Ref);

    World* world_;
    using Vars = std::deque<std::pair<Def*, Def*>>;
    Vars vars_;
    MutSet done_;
};

inline bool equiv(Ref d1, Ref d2) { return Checker(d1->world()).equiv_(d1, d2) != Equiv::No; }
inline bool assignable(Ref type, Ref value) { return Checker(value->world()).assignable_(type, value) != Equiv::No; }
inline const Def* is_uniform(Defs defs) {
    if (defs.empty()) return nullptr;
    return Checker(defs.front()->world()).is_uniform(defs);
}

} // namespace thorin
