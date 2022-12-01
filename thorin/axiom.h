#pragma once

#include "thorin/lam.h"

#include "thorin/util/assert.h"

namespace thorin {

class Axiom : public Def {
private:
    Axiom(NormalizeFn, u8 curry, u8 trip, const Def* type, dialect_t, tag_t, sub_t, const Def* dbg);

public:
    /// @name normalization
    ///@{
    /// For a curried App of an Axiom, you only want to trigger normalization at specific spots.
    /// For this reason, Thorin maintains a Def::curry_ counter that each App decrements.
    /// The Axiom::normalizer() will be triggered when Axiom::curry() becomes `0`.
    /// These are also the spots that you can thorin::match/thorin::force/Match.
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
    NormalizeFn normalizer() const { return normalizer_; }
    u8 curry() const { return curry_; }
    u8 trip() const { return trip_; }

    /// Yields currying counter of @p def.
    /// @returns `{nullptr, 0, 0}` if no Axiom is present.
    static std::tuple<const Axiom*, u8, u8> get(const Def* def);

    static std::pair<u8, u8> infer_curry_and_trip(const Def* type);
    static constexpr u8 Trip_End = u8(-1);
    ///@}

    /// @name Axiom name
    ///@{
    /// Anatomy of an Axiom name:
    /// ```
    /// %dialect.tag.sub
    /// |  48   | 8 | 8 | <-- Number of bits per field.
    /// ```
    /// * Def::name() retrieves the full name as `std::string`.
    /// * Def::flags() retrieves the full name as Axiom::mangle%d 64-bit integer.

    /// Yields the `dialect` part of the name as integer.
    /// It consists of 48 relevant bits that are returned in the highest 6 bytes of a 64-bit integer.
    dialect_t dialect() const { return flags() & Global_Dialect; }

    /// Yields the `tag` part of the name as integer.
    tag_t tag() const { return tag_t((flags() & 0x0000'0000'0000'ff00_u64) >> 8_u64); }

    /// Yields the `sub` part of the name as integer.
    sub_t sub() const { return sub_t(flags() & 0x0000'0000'0000'00ff_u64); }

    /// Includes Axiom::dialect() and Axiom::tag() but **not** Axiom::sub.
    flags_t base() const { return flags() & ~0xff_u64; }
    ///@}

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    /// @name Mangling Dialect Name
    ///@{
    static constexpr size_t Max_Dialect_Size  = 8;
    static constexpr dialect_t Global_Dialect = 0xffff'ffff'ffff'0000_u64;

    /// Mangles @p s into a dense 48-bit representation.
    /// The layout is as follows:
    /// ```
    /// |---7--||---6--||---5--||---4--||---3--||---2--||---1--||---0--|
    /// 7654321076543210765432107654321076543210765432107654321076543210
    /// Char67Char66Char65Char64Char63Char62Char61Char60|---reserved---|
    /// ```
    /// The `reserved` part is used for the Axiom::tag and the Axiom::sub.
    /// Each `Char6x` is 6-bit wide and hence a dialect name has at most Axiom::Max_Dialect_Size = 8 chars.
    /// It uses this encoding:
    /// | `Char6` | ASCII   |
    /// |---------|---------|
    /// | 1:      | `_`     |
    /// | 2-27:   | `a`-`z` |
    /// | 28-53:  | `A`-`Z` |
    /// | 54-63:  | `0`-`9` |
    /// The 0 is special and marks the end of the name if the name has less than 8 chars.
    /// @returns `std::nullopt` if encoding is not possible.
    static std::optional<dialect_t> mangle(std::string_view s);

    /// Reverts an Axiom::mangle%d string to a `std::string`.
    /// Ignores lower 16-bit of @p u.
    static std::string demangle(dialect_t u);

    static std::optional<std::array<std::string_view, 3>> split(std::string_view);
    ///@}

    /// @name Helpers for Matching
    ///@{
    /// These are set via template specialization.

    /// Number of Axiom::sub%tags.
    template<class Id>
    static constexpr size_t Num = size_t(-1);

    /// @sa Axiom::base.
    template<class Id>
    static constexpr flags_t Base = flags_t(-1);

    /// Type of Match::def_.
    template<class T>
    struct Match {
        using type = App;
    };
    ///@}

    static constexpr auto Node = Node::Axiom;
    friend class World;
};

// clang-format off
template<class Id> concept axiom_with_subs    = Axiom::Num<Id> != 0;
template<class Id> concept axiom_without_subs = Axiom::Num<Id> == 0;
// clang-format on

template<class Id, class D>
class Match {
    static_assert(Axiom::Num<Id> != size_t(-1), "invalid number of sub tags");
    static_assert(Axiom::Base<Id> != flags_t(-1), "invalid axiom base");

public:
    Match() = default;
    Match(const Axiom* axiom, const D* def)
        : axiom_(axiom)
        , def_(def) {}

    /// @name getters
    ///@{
    const Axiom* axiom() const { return axiom_; }
    const D* operator->() const { return def_; }
    operator const D*() const { return def_; }
    explicit operator bool() { return axiom_ != nullptr; }
    ///@}

    /// @name Axiom name
    ///@{
    auto dialect() const { return axiom()->dialect(); } ///< @sa Axiom::dialect.
    auto tag() const { return axiom()->tag(); }         ///< @sa Axiom::tag.
    auto sub() const { return axiom()->sub(); }         ///< @sa Axiom::sub.
    auto base() const { return axiom()->sub(); }        ///< @sa Axiom::base.
    auto id() const { return Id(axiom()->flags()); }    ///< Axiom::flags cast to @p Id.
    ///@}

private:
    const Axiom* axiom_ = nullptr;
    const D* def_       = nullptr;
};

/// @name match/force
///@{
template<class Id, bool DynCast = true>
auto match(const Def* def) {
    using D                = typename Axiom::Match<Id>::type;
    auto [axiom, curry, _] = Axiom::get(def);
    bool cond              = axiom && curry == 0 && axiom->base() == Axiom::Base<Id>;

    if constexpr (DynCast) return cond ? Match<Id, D>(axiom, def->as<D>()) : Match<Id, D>();
    assert(cond && "assumed to be correct axiom");
    return Match<Id, D>(axiom, def->as<D>());
}

template<class Id, bool DynCast = true>
auto match(Id id, const Def* def) {
    using D                = typename Axiom::Match<Id>::type;
    auto [axiom, curry, _] = Axiom::get(def);
    bool cond              = axiom && curry == 0 && axiom->flags() == (flags_t)id;

    if constexpr (DynCast) return cond ? Match<Id, D>(axiom, def->as<D>()) : Match<Id, D>();
    assert(cond && "assumed to be correct axiom");
    return Match<Id, D>(axiom, def->as<D>());
}

// clang-format off
template<class Id> auto force(       const Def* def) { return match<Id, false>(    def); }
template<class Id> auto force(Id id, const Def* def) { return match<Id, false>(id, def); }
///@}

/// @name is_commutative/is_associative
///@{
template<class Id> constexpr bool is_commutative(Id) { return false; }
/// @warning By default we assume that any commutative operation is also associative.
/// Please provide a proper specialization if this is not the case.
template<class Id> constexpr bool is_associative(Id id) { return is_commutative(id); }
///@}
// clang-format on

} // namespace thorin
