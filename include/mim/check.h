#pragma once

#include "mim/def.h"

namespace mim {

/// This node is a hole in the IR that is inferred by its context later on.
/// It is modelled as a *mut*able Def.
/// If inference was successful, it's Infer::op will be set to the inferred Def.
class Infer : public Def, public Setters<Infer> {
private:
    Infer(const Def* type)
        : Def(Node, type, 1, 0) {}

public:
    using Setters<Infer>::set;

    /// @name op
    ///@{
    const Def* op() const { return Def::op(0); }
    Infer* set(const Def* op) { return Def::set(0, op)->as<Infer>(); }
    Infer* reset(const Def* op) { return Def::reset(0, op)->as<Infer>(); }
    Infer* unset() { return Def::unset()->as<Infer>(); }
    ///@}

    /// [Union-Find](https://en.wikipedia.org/wiki/Disjoint-set_data_structure) to unify Infer nodes.
    /// Def::flags is used to keep track of rank for
    /// [Union by rank](https://en.wikipedia.org/wiki/Disjoint-set_data_structure#Union_by_rank).
    static const Def* find(const Def*);

    Infer* stub(Ref type) { return stub_(world(), type)->set(dbg()); }
    /// If unset, explode to Tuple.
    /// @returns the new Tuple, or `this` if unsuccessful.
    Ref tuplefy();

    static constexpr auto Node = Node::Infer;

private:
    flags_t rank() const { return flags(); }
    flags_t& rank() { return flags_; }

    Ref rebuild_(World&, Ref, Defs) const override;
    Infer* stub_(World&, Ref) override;

    friend class World;
    friend class Checker;
};

class Checker {
public:
    Checker(World& world)
        : world_(world) {}

    World& world() { return world_; }

    enum Mode {
        /// In Mode::Check, type inference is happening and Infer%s will be resolved, if possible.
        /// Also, two *free* but *different* Var%s **are** considered α-equivalent.
        Check,
        /// In Mode::Test, no type inference is happening and Infer%s will not be touched.
        /// Also, Two *free* but *different* Var%s are **not** considered α-equivalent.
        Test,
    };

    template<Mode mode> static bool alpha(Ref d1, Ref d2) { return Checker(d1->world()).alpha_<mode>(d1, d2); }

    /// Can @p value be assigned to sth of @p type?
    /// @note This is different from `equiv(type, value->type())` since @p type may be dependent.
    [[nodiscard]] static Ref assignable(Ref type, Ref value) { return Checker(type->world()).assignable_(type, value); }

    /// Yields `defs.front()`, if all @p defs are Check::alpha-equivalent (`Mode::Test`) and `nullptr` otherwise.
    static Ref is_uniform(Defs defs);

private:
#ifdef MIM_ENABLE_CHECKS
    template<Mode> bool fail();
    Ref fail();
#else
    template<Mode> bool fail() { return false; }
    Ref fail() { return {}; }
#endif

    template<Mode> bool alpha_(Ref d1, Ref d2);
    template<Mode> bool alpha_internal(Ref, Ref);
    [[nodiscard]] Ref assignable_(Ref type, Ref value);

    World& world_;
    MutMap<const Def*> binders_;
    fe::Arena arena_;
};

} // namespace mim
