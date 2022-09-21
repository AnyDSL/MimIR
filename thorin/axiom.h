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
    static std::optional<dialect_t> mangle(std::string_view s);

    /// Reverts an Axiom::mangle%d string to a `std::string`.
    /// Ignores lower 16-bit of @p u.
    static std::string demangle(dialect_t u);

    static std::optional<std::array<std::string_view, 3>> split(std::string_view);
    ///@}

    static std::tuple<const Axiom*, u16> get(const Def*);

    static constexpr auto Node = Node::Axiom;
    friend class World;
};

template<class AxTag>
concept axiom_with_sub_tags = requires(AxTag t) {
    AxTag::Axiom_Base;
};

template<class AxTag>
concept axiom_without_sub_tags = requires(AxTag t) {
    AxTag::Axiom_Id;
};

template<class AxTag>
concept axiom_from_dialect = axiom_with_sub_tags<AxTag> || axiom_without_sub_tags<AxTag>;

template<class AxTag>
concept axiom_from_thorin = !axiom_from_dialect<AxTag>;

template<class T, class D>
class Match {
public:
    Match()
        : axiom_(nullptr)
        , def_(nullptr) {}
    Match(const Axiom* axiom, const D* def)
        : axiom_(axiom)
        , def_(def) {}

    const Axiom* axiom() const { return axiom_; }
    tag_t tag() const { return axiom()->tag(); }
    auto sub() const {
        if constexpr (axiom_from_dialect<T>)
            return axiom()->sub();
        else
            return T(axiom()->sub());
    }
    auto flags() const {
        if constexpr (axiom_from_dialect<T>)
            return T(axiom()->flags());
        else
            return axiom()->flags();
    }
    void clear() {
        axiom_ = nullptr;
        def_   = nullptr;
    }

    const D* operator->() const { return def_; }
    operator const D*() const { return def_; }
    explicit operator bool() { return axiom_ != nullptr; }

private:
    const Axiom* axiom_;
    const D* def_;
};

template<tag_t>
struct Tag2Def_ {
    using type = App;
};
template<>
struct Tag2Def_<Tag::Mem> {
    using type = Axiom;
};
template<tag_t t>
using Tag2Def = typename Tag2Def_<t>::type;

template<tag_t t>
Match<Tag2Enum<t>, Tag2Def<t>> isa(const Def* def) {
    auto [axiom, curry] = Axiom::get(def);
    if (axiom && axiom->dialect() == Axiom::Global_Dialect && axiom->tag() == t && curry == 0)
        return {axiom, def->as<Tag2Def<t>>()};
    return {};
}

template<tag_t t>
Match<Tag2Enum<t>, Tag2Def<t>> isa(Tag2Enum<t> tag, const Def* def) {
    auto [axiom, curry] = Axiom::get(def);
    if (axiom && axiom->dialect() == Axiom::Global_Dialect && axiom->tag() == t && axiom->tag() == tag_t(tag) &&
        curry == 0)
        return {axiom, def->as<Tag2Def<t>>()};
    return {};
}

template<tag_t t>
Match<Tag2Enum<t>, Tag2Def<t>> as(const Def* d) {
    assert(isa<t>(d));
    return {std::get<0>(Axiom::get(d)), d->as<App>()};
}
template<tag_t t>
Match<Tag2Enum<t>, Tag2Def<t>> as(Tag2Enum<t> f, const Def* d) {
    assert((isa<t>(f, d)));
    return {std::get<0>(Axiom::get(d)), d->as<App>()};
}

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
template<class AxTag>
struct Enum2DefImpl {
    using type = App;
};

template<class AxTag>
using Enum2Def = typename Enum2DefImpl<AxTag>::type;

template<class AxTag>
constexpr AxTag base_value() {
    if constexpr (axiom_with_sub_tags<AxTag>)
        return AxTag::Axiom_Base;
    else
        return AxTag::Axiom_Id;
}

} // namespace detail

template<class AxTag, bool Check = true>
Match<AxTag, detail::Enum2Def<AxTag>> match(const Def* def) {
    auto [axiom, curry] = Axiom::get(def);
    if constexpr (Check) {
        if (axiom && (axiom->flags() & ~0xFF_u64) == detail::base_value<AxTag>() && curry == 0)
            return {axiom, def->as<detail::Enum2Def<AxTag>>()};
        return {};
    }
    assert(axiom && (axiom->flags() & ~0xFF_u64) == detail::base_value<AxTag>() && curry == 0 &&
           "assumed to be correct axiom");
    return {axiom, def->as<detail::Enum2Def<AxTag>>()};
}

template<class AxTag, bool Check = true>
Match<AxTag, detail::Enum2Def<AxTag>> match(AxTag sub, const Def* def) {
    auto [axiom, curry] = Axiom::get(def);
    if constexpr (Check) {
        if (axiom && axiom->flags() == sub && curry == 0) return {axiom, def->as<detail::Enum2Def<AxTag>>()};
        return {};
    }
    assert(axiom && axiom->flags() == sub && curry == 0 && "assumed to be correct axiom");
    return {axiom, def->as<detail::Enum2Def<AxTag>>()};
}

} // namespace thorin
