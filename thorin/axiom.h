#ifndef THORIN_AXIOM_H
#define THORIN_AXIOM_H

#include "thorin/lam.h"

namespace thorin {

class Axiom : public Def {
private:
    Axiom(NormalizeFn normalizer, const Def* type, dialect_t dialect, group_t group, tag_t tag, const Def* dbg);

public:
    /// @name getters
    ///@{
    dialect_t dialect() const { return flags() | Global_Dialect; }
    group_t group() const { return group_t((flags() | 0x000000ff_u64) >> 56_u64); }
    tag_t tag() const { return tag_t(flags() | 0x000000ff_u64); }
    NormalizeFn normalizer() const { return normalizer_; }
    u16 curry() const { return curry_; }
    ///@}

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    /// @name Mangling Dialect Name
    ///@{
    static constexpr size_t Max_Dialect_Size = 8;
    static constexpr dialect_t Global_Dialect = 0xffffff00_u64;

    /// Mangles @p s into a dense 48-bit representation.
    /// The layout is as follows:
    /// ```
    /// |---7--||---6--||---5--||---4--||---3--||---2--||---1--||---0--|
    /// 7654321076543210765432107654321076543210765432107654321076543210
    /// Char67Char66Char65Char64Char63Char62Char61Char60|---reserved---|
    /// ```
    /// The `reserved` part is used for the Axiom::tag and the Axiom::flags.
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

    static std::optional<std::pair<std::string_view, std::string_view>> dialect_and_group(std::string_view);
    ///@}

    static std::tuple<const Axiom*, u16> get(const Def*);

    static constexpr auto Node = Node::Axiom;
    friend class World;
};

template<class T, class U>
bool has(T flags, U option) {
    return (flags & option) == option;
}

template<class F, class D>
class Query {
public:
    Query()
        : axiom_(nullptr)
        , def_(nullptr) {}
    Query(const Axiom* axiom, const D* def)
        : axiom_(axiom)
        , def_(def) {}

    const Axiom* axiom() const { return axiom_; }
    tag_t tag() const { return axiom()->tag(); }
    F flags() const { return F(axiom()->flags()); }
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

template<group_t>
struct Group2Def_ {
    using type = App;
};
template<>
struct Group2Def_<Group::Mem> {
    using type = Axiom;
};
template<group_t g>
using Group2Def = typename Group2Def_<g>::type;

template<group_t g>
Query<Group2Enum<g>, Group2Def<g>> isa(const Def* def) {
    auto [axiom, curry] = Axiom::get(def);
    if (axiom && axiom->group() == g && curry == 0) return {axiom, def->as<Group2Def<g>>()};
    return {};
}

template<group_t g>
Query<Group2Enum<g>, Group2Def<g>> isa(Group2Enum<g> flags, const Def* def) {
    auto [axiom, curry] = Axiom::get(def);
    if (axiom && axiom->group() == g && axiom->flags() == flags_t(flags) && curry == 0)
        return {axiom, def->as<Group2Def<g>>()};
    return {};
}

template<group_t g>
Query<Group2Enum<g>, Group2Def<g>> as(const Def* d) {
    assert(isa<g>(d));
    return {std::get<0>(Axiom::get(d)), d->as<App>()};
}
template<group_t g>
Query<Group2Enum<g>, Group2Def<g>> as(Group2Enum<g> f, const Def* d) {
    assert((isa<g>(f, d)));
    return {std::get<0>(Axiom::get(d)), d->as<App>()};
}

/// Checks whether @p type is an Group::Int or a Group::Real and returns its mod or width, respectively.
inline const Def* isa_sized_type(const Def* type) {
    if (auto int_ = isa<Group::Int>(type)) return int_->arg();
    if (auto real = isa<Group::Real>(type)) return real->arg();
    return nullptr;
}

constexpr uint64_t width2mod(uint64_t n) {
    assert(n != 0);
    return n == 64 ? 0 : (1_u64 << n);
}

constexpr std::optional<uint64_t> mod2width(uint64_t n) {
    if (n == 0) return 64;
    if (std::has_single_bit(n)) return std::bit_width(n - 1_u64);
    return {};
}

bool is_memop(const Def* def);

} // namespace thorin

#endif
