#pragma once

#include "thorin/lam.h"

#include "thorin/util/assert.h"

namespace thorin {

class Axiom : public Def {
private:
    Axiom(NormalizeFn normalizer, const Def* type, dialect_t dialect, tag_t tag, sub_t sub, const Def* dbg);

public:
    /// @name getters
    ///@{
    dialect_t dialect() const { return flags() & Global_Dialect; }
    tag_t tag() const { return tag_t((flags() & 0x0000'0000'0000'ff00_u64) >> 8_u64); }
    sub_t sub() const { return sub_t(flags() & 0x0000'0000'0000'00ff_u64); }
    /// Includes Axiom::dialect and Axiom::tag but **not** Axiom::sub.
    flags_t base() const { return flags() & ~0xff_u64; }
    NormalizeFn normalizer() const { return normalizer_; }
    u16 curry() const { return curry_; }
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
    static std::optional<dialect_t>
    mangle(std::string_view s);

    /// Reverts an Axiom::mangle%d string to a `std::string`.
    /// Ignores lower 16-bit of @p u.
    static std::string demangle(dialect_t u);

    static std::optional<std::array<std::string_view, 3>> split(std::string_view);
    ///@}

    /// @name Helpers for Matching
    ///@{
    /// These are set via template specialization.

    /// Number of Axiom::sub%tags.
    template<class T>
    static constexpr size_t Num = size_t(-1);

    /// Includes Axiom::dialect and Axiom::tag but **not** Axiom::sub.
    template<class T>
    static constexpr flags_t Base = flags_t(-1);

    /// Type of Match::def_.
    template<class T>
    struct Match {
        using type = App;
    };
    ///@}

    static std::tuple<const Axiom*, u16> get(const Def*);

    static constexpr auto Node = Node::Axiom;
    friend class World;
};

// clang-format off
template<class T> concept axiom_with_subs    = Axiom::Num<T> != 0;
template<class T> concept axiom_without_subs = Axiom::Num<T> == 0;
// clang-format on

template<class T, class D>
class Match {
    static_assert(Axiom::Num<T> != size_t(-1), "invalid number of sub tags");
    static_assert(Axiom::Base<T> != flags_t(-1), "invalid axiom base");

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

    /// @name name
    ///@{
    auto dialect() const { return axiom()->dialect(); }
    auto tag() const { return axiom()->tag(); }
    auto sub() const { return axiom()->sub(); }
    auto id() const { return T(axiom()->flags()); }
    ///@}

private:
    const Axiom* axiom_ = nullptr;
    const D* def_       = nullptr;
};

constexpr uint64_t bitwidth2size(uint64_t n) {
    assert(n != 0);
    return n == 64 ? 0 : (1_u64 << n);
}

constexpr std::optional<uint64_t> size2bitwidth(uint64_t n) {
    if (n == 0) return 64;
    if (std::has_single_bit(n)) return std::bit_width(n - 1_u64);
    return {};
}

namespace detail {

template<class T, bool Check, bool Base>
auto match(flags_t sub, const Def* def) {
    static_assert(Axiom::Num<T> != size_t(-1), "invalid number of sub tags");
    static_assert(Axiom::Base<T> != flags_t(-1), "invalid axiom base");

    using D = typename Axiom::Match<T>::type;

    auto [axiom, curry] = Axiom::get(def);
    bool cond = axiom && curry == 0 && (Base ? axiom->base() : axiom->flags()) == sub;
    if constexpr (Check) {
        if (cond) return Match<T, D>(axiom, def->as<D>());
        return Match<T, D>();
    }
    assert(cond && "assumed to be correct axiom");
    return Match<T, D>(axiom, def->as<D>());
}

} // namespace detail

// clang-format off
template<class T, bool Check = true>
auto match(const Def* def) { return detail::match<T, Check, true>(Axiom::Base<T>, def); }

template<class T, bool Check = true>
auto match(T sub, const Def* def) { return detail::match<T, Check, false>((flags_t)sub, def); }
// clang-format on

} // namespace thorin
