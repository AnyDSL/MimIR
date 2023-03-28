#pragma once

#include <deque>

#include "thorin/def.h"

namespace thorin {

/// This node is a hole in the IR that is inferred by its context later on.
/// It is modelled as a *nom*inal Def.
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
    ///@}

    /// @name union-find
    ///@{
    /// [Union-Find](https://en.wikipedia.org/wiki/Disjoint-set_data_structure) to unify Infer nodes.
    /// Def::flags is used to keep track of rank for
    /// [Union by rank](https://en.wikipedia.org/wiki/Disjoint-set_data_structure#Union_by_rank).
    static const Def* find(const Def*);

private:
    flags_t rank() const { return flags(); }
    flags_t& rank() { return flags_; }
    ///@}

    THORIN_DEF_MIXIN(Infer)
    Infer* stub_(World&, Ref) override;
    friend struct Checker;
};

/// Are @p d1 and @p d2 α-equivalent?
bool equiv(Ref d1, Ref d2);

/// Can @p value be assigned to sth of @p type?
/// @note This is different from `equiv(type, value->type())` since @p type may be dependent.
bool assignable(Ref type, Ref value);

/// Yields `defs.front()`, if all @p defs are α-equiv%alent and `nullptr` otherwise.
Ref is_uniform(Defs defs);

} // namespace thorin
