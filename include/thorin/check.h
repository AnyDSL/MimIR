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

    /// Eliminate Infer%s that may have been resolved in the meantime by rebuilding.
    /// @returns `true`, if one of the arguements was in fact updated.
    static bool eliminate(Vector<Ref*>);
    static bool should_eliminate(Ref def) { return def->isa_imm() && def->has_dep(Dep::Infer); }

    /// [Union-Find](https://en.wikipedia.org/wiki/Disjoint-set_data_structure) to unify Infer nodes.
    /// Def::flags is used to keep track of rank for
    /// [Union by rank](https://en.wikipedia.org/wiki/Disjoint-set_data_structure#Union_by_rank).
    static const Def* find(const Def*);

    Infer* stub(Ref type) { return stub_(world(), type)->set(dbg()); }

private:
    Infer* stub_(World&, Ref) override;
    flags_t rank() const { return flags(); }
    flags_t& rank() { return flags_; }

    THORIN_DEF_MIXIN(Infer)
    friend class Check;
};

class Check {
public:
    Check(World& world)
        : world_(world) {}

    World& world() { return world_; }

    /// Are d1 and d2 α-equivalent?
    /// * In @p infer mode, type inference is happening and Infer%s will be resolved, if possible.
    ///     Also, two *free* but *different* Var%s **are** considered α-equivalent.
    /// * When **not* in @p infer mode, no type inference is happening and Infer%s will not be touched.
    ///     Also, Two *free* but *different* Var%s are **not** considered α-equivalent.
    template<bool infer = true> static bool alpha(Ref d1, Ref d2) { return Check(d1->world()).alpha_<infer>(d1, d2); }

    /// Can @p value be assigned to sth of @p type?
    /// @note This is different from `equiv(type, value->type())` since @p type may be dependent.
    static bool assignable(Ref type, Ref value) { return Check(type->world()).assignable_(type, value); }

    /// Yields `defs.front()`, if all @p defs are Check::alpha-equivalent (`infer = false`) and `nullptr` otherwise.
    static Ref is_uniform(Defs defs);

private:
#ifdef THORIN_ENABLE_CHECKS
    template<bool> bool fail();
#else
    template<bool> bool fail() { return false; }
#endif

    template<bool infer> bool alpha_(Ref d1, Ref d2);
    template<bool infer> bool alpha_internal(Ref, Ref);
    bool assignable_(Ref type, Ref value);

    World& world_;
    using Vars = MutMap<Def*>;
    Vars vars_;
    MutMap<Ref> done_;
};

} // namespace thorin
