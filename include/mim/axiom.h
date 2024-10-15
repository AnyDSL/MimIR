#pragma once

#include <fe/assert.h>

#include "mim/plugin.h"

namespace mim {

class Axiom : public Def {
private:
    Axiom(NormalizeFn, u8 curry, u8 trip, const Def* type, plugin_t, tag_t, sub_t);

public:
    /// @name Normalization
    /// @anchor normalization
    /// For a curried App of an Axiom, you only want to trigger normalization at specific spots.
    /// For this reason, MimIR maintains a Def::curry_ counter that each App decrements.
    /// The Axiom::normalizer() will be triggered when Axiom::curry() becomes `0`.
    /// These are also the spots that you can mim::match / mim::force / mim::Match.
    /// After that, the counter will be set to Axiom::trip().
    /// E.g., let's say an Axiom has this type:
    /// ```
    /// A -> B -> C -> D -> E
    ///           ^         |
    ///           |         |
    ///           +---------+
    /// ```
    /// Using an initial value as `5` for Axiom::curry and `3` as Axiom::trip has the effect that here
    /// ```
    /// x a b c1 d1 e1 c2 d2 e2 c3 d3 e3
    /// ```
    /// the Axiom::normalizer will be triggered after App'ing `e1`, `e2`, and `e3`.
    ///@{
    NormalizeFn normalizer() const { return normalizer_; }
    u8 curry() const { return curry_; }
    u8 trip() const { return trip_; }

    /// Yields currying counter of @p def.
    /// @returns `{nullptr, 0, 0}` if no Axiom is present.
    static std::tuple<const Axiom*, u8, u8> get(const Def* def);

    static std::pair<u8, u8> infer_curry_and_trip(const Def* type);
    static constexpr u8 Trip_End = u8(-1);
    ///@}

    /// @name Annex Name
    /// @anchor anatomy
    /// @see annex_name "Annex Name"
    ///@{
    plugin_t plugin() const { return Annex::flags2plugin(flags()); }
    tag_t tag() const { return Annex::flags2tag(flags()); }
    sub_t sub() const { return Annex::flags2sub(flags()); }
    flags_t base() const { return Annex::flags2base(flags()); }
    ///@}

    /// Type of Match::def_.
    template<class T> struct Match {
        using type = App;
    };

    MIM_DEF_MIXIN(Axiom)
};

// clang-format off
template<class Id> concept annex_with_subs    = Annex::Num<Id> != 0;
template<class Id> concept annex_without_subs = Annex::Num<Id> == 0;
// clang-format on

template<class Id, class D> class Match {
    static_assert(Annex::Num<Id> != size_t(-1), "invalid number of sub tags");
    static_assert(Annex::Base<Id> != flags_t(-1), "invalid axiom base");

public:
    Match() = default;
    Match(const Axiom* axiom, const D* def)
        : axiom_(axiom)
        , def_(def) {}

    /// @name Getters
    ///@{
    const Axiom* axiom() const { return axiom_; }
    const D* operator->() const { return def_; }
    operator const D*() const { return def_; }
    explicit operator bool() { return axiom_ != nullptr; }
    ///@}

    /// @name Axiom Name
    /// @see annex_name "Annex Name"
    ///@{
    auto plugin() const { return axiom()->plugin(); } ///< @see Axiom::plugin.
    auto tag() const { return axiom()->tag(); }       ///< @see Axiom::tag.
    auto sub() const { return axiom()->sub(); }       ///< @see Axiom::sub.
    auto base() const { return axiom()->sub(); }      ///< @see exiom::base.
    auto id() const { return Id(axiom()->flags()); }  ///< Axiom::flags cast to @p Id.
    ///@}

private:
    const Axiom* axiom_ = nullptr;
    const D* def_       = nullptr;
};

/// @name match/force
///@{
/// @see @ref cast_axiom
template<class Id, bool DynCast = true> auto match(Ref def) {
    using D                = typename Axiom::Match<Id>::type;
    auto [axiom, curry, _] = Axiom::get(def);
    bool cond              = axiom && curry == 0 && axiom->base() == Annex::Base<Id>;

    if constexpr (DynCast) return cond ? Match<Id, D>(axiom, def->as<D>()) : Match<Id, D>();
    assert(cond && "assumed to be correct axiom");
    return Match<Id, D>(axiom, def->as<D>());
}

template<class Id, bool DynCast = true> auto match(Id id, Ref def) {
    using D                = typename Axiom::Match<Id>::type;
    auto [axiom, curry, _] = Axiom::get(def);
    bool cond              = axiom && curry == 0 && axiom->flags() == (flags_t)id;

    if constexpr (DynCast) return cond ? Match<Id, D>(axiom, def->as<D>()) : Match<Id, D>();
    assert(cond && "assumed to be correct axiom");
    return Match<Id, D>(axiom, def->as<D>());
}

// clang-format off
template<class Id> auto force(       Ref def) { return match<Id, false>(    def); }
template<class Id> auto force(Id id, Ref def) { return match<Id, false>(id, def); }
///@}

/// @name is_commutative/is_associative
///@{
template<class Id> constexpr bool is_commutative(Id) { return false; }
/// @warning By default we assume that any commutative operation is also associative.
/// Please provide a proper specialization if this is not the case.
template<class Id> constexpr bool is_associative(Id id) { return is_commutative(id); }
///@}
// clang-format on

} // namespace mim
