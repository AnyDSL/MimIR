#pragma once

#include "mim/def.h"

namespace mim {

/// This node is a hole in the IR that is inferred by its context later on.
/// It is modelled as a *mutable* Def.
/// If inference was successful, it's Hole::op will be set to the inferred Def.
class Hole : public Def, public Setters<Hole> {
private:
    Hole(const Def* type)
        : Def(Node, type, 1, 0) {}

public:
    using Setters<Hole>::set;

    /// @name op
    ///@{
    const Def* op() const { return Def::op(0); }
    Hole* set(const Def* op) {
        assert(op != this);
        return Def::set(0, op)->as<Hole>();
    }
    Hole* unset() { return Def::unset()->as<Hole>(); }
    ///@}

    /// [Union-Find](https://en.wikipedia.org/wiki/Disjoint-set_data_structure) to unify Hole%s.
    /// Def::flags is used to keep track of rank for
    /// [Union by rank](https://en.wikipedia.org/wiki/Disjoint-set_data_structure#Union_by_rank).
    static const Def* find(const Def*);

    Hole* stub(const Def* type) { return stub_(world(), type)->set(dbg()); }
    /// If unset, explode to Tuple.
    /// @returns the new Tuple, or `this` if unsuccessful.
    const Def* tuplefy();

    static constexpr auto Node = mim::Node::Hole;

private:
    flags_t rank() const { return flags(); }
    flags_t& rank() { return flags_; }

    const Def* rebuild_(World&, const Def*, Defs) const override;
    Hole* stub_(World&, const Def*) override;

    friend class World;
    friend class Checker;
};

class Checker {
public:
    Checker(World& world)
        : world_(world) {}

    World& world() { return world_; }

    enum Mode {
        /// In Mode::Check, type inference is happening and Hole%s will be resolved, if possible.
        /// Also, two *free* but *different* Var%s **are** considered α-equivalent.
        Check,
        /// In Mode::Test, no type inference is happening and Hole%s will not be touched.
        /// Also, Two *free* but *different* Var%s are **not** considered α-equivalent.
        Test,
    };

    template<Mode mode> static bool alpha(const Def* d1, const Def* d2) {
        return Checker(d1->world()).alpha_<mode>(d1, d2);
    }

    /// Can @p value be assigned to sth of @p type?
    /// @note This is different from `equiv(type, value->type())` since @p type may be dependent.
    [[nodiscard]] static const Def* assignable(const Def* type, const Def* value) {
        return Checker(type->world()).assignable_(type, value);
    }

    /// Yields `defs.front()`, if all @p defs are Check::alpha-equivalent (`Mode::Test`) and `nullptr` otherwise.
    static const Def* is_uniform(Defs defs);

private:
#ifdef MIM_ENABLE_CHECKS
    template<Mode> bool fail();
    const Def* fail();
#else
    template<Mode> bool fail() { return false; }
    const Def* fail() { return {}; }
#endif

    template<Mode> bool alpha_(const Def* d1, const Def* d2);
    template<Mode> bool alpha_internal(const Def*, const Def*);
    [[nodiscard]] const Def* assignable_(const Def* type, const Def* value);

    World& world_;
    MutMap<const Def*> binders_;
    fe::Arena arena_;
};

} // namespace mim
