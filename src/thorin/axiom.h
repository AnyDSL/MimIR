#ifndef THORIN_AXIOM_H
#define THORIN_AXIOM_H

#include "thorin/lam.h"

namespace thorin {

class Axiom : public Def {
private:
    Axiom(NormalizeFn normalizer, const Def* type, tag_t tag, flags_t flags, const Def* dbg);

public:
    /// @name misc getters
    ///@{
    tag_t tag() const { return tag_t(fields() >> 32_u64); }
    flags_t flags() const { return flags_t(fields()); }
    NormalizeFn normalizer() const { return normalizer_depth_.ptr(); }
    u16 currying_depth() const { return normalizer_depth_.index(); }
    ///@}

    /// @name virtual methods
    ///@{
    const Def* rebuild(World&, const Def*, Defs, const Def*) const override;
    ///@}

    /// @name mangling dialect name
    ///@{

    /// Mangles @p s into a dense 48-bit representation.
    /// The layout is as follows:
    /// ```
    /// |---7--||---6--||---5--||---4--||---3--||---2--||---1--||---0--|
    /// 7654321076543210765432107654321076543210765432107654321076543210
    /// Char67Char66Char65Char64Char63Char62Char61Char60|---reserved---|
    /// ```
    /// The `reserved` part is used for the Axiom::tag and the Axiom::flags.
    /// Each `Char6x` is 6-bit wide and uses this encoding:
    /// | `Char6` | ASCII   |
    /// |---------|---------|
    /// | 1:      | `_`     |
    /// | 2-27:   | `a`-`z` |
    /// | 28-53:  | `A`-`Z` |
    /// | 54-63:  | `0`-`9` |
    /// @return returns `std::nullopt` if encoding is not possible.
    static std::optional<u64> mangle(std::string_view s);

    /// Reverts a mangle%d string to a `std::string`.
    /// Ignores lower 16-bit of @p u.
    static std::string demangle(u64 u);
    ///@}

    static std::tuple<const Axiom*, u16> get(const Def*);

    static constexpr size_t Max_Dialect_Size = 8;
    static constexpr auto Node               = Node::Axiom;
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

template<tag_t tag>
struct Tag2Def_ {
    using type = App;
};
template<>
struct Tag2Def_<Tag::Mem> {
    using type = Axiom;
};
template<tag_t tag>
using Tag2Def = typename Tag2Def_<tag>::type;

template<tag_t tag>
Query<Tag2Enum<tag>, Tag2Def<tag>> isa(const Def* def) {
    auto [axiom, currying_depth] = Axiom::get(def);
    if (axiom && axiom->tag() == tag && currying_depth == 0) return {axiom, def->as<Tag2Def<tag>>()};
    return {};
}

template<tag_t tag>
Query<Tag2Enum<tag>, Tag2Def<tag>> isa(Tag2Enum<tag> flags, const Def* def) {
    auto [axiom, currying_depth] = Axiom::get(def);
    if (axiom && axiom->tag() == tag && axiom->flags() == flags_t(flags) && currying_depth == 0)
        return {axiom, def->as<Tag2Def<tag>>()};
    return {};
}

template<tag_t t>
Query<Tag2Enum<t>, Tag2Def<t>> as(const Def* d) {
    assert(isa<t>(d));
    return {std::get<0>(Axiom::get(d)), d->as<App>()};
}
template<tag_t t>
Query<Tag2Enum<t>, Tag2Def<t>> as(Tag2Enum<t> f, const Def* d) {
    assert((isa<t>(f, d)));
    return {std::get<0>(Axiom::get(d)), d->as<App>()};
}

/// Checks whether @p type is an Int or a Real and returns its mod or width, respectively.
inline const Def* isa_sized_type(const Def* type) {
    if (auto int_ = isa<Tag::Int>(type)) return int_->arg();
    if (auto real = isa<Tag::Real>(type)) return real->arg();
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
