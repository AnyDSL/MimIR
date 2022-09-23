#pragma once

#include "thorin/def.h"
#include "thorin/world.h"

namespace thorin {

class Def;

const Def* normalize_bit(const Def*, const Def*, const Def*, const Def*);
const Def* normalize_bitcast(const Def*, const Def*, const Def*, const Def*);
const Def* normalize_zip(const Def*, const Def*, const Def*, const Def*);

template<Bit>
const Def* normalize_Bit(const Def*, const Def*, const Def*, const Def*);
template<Shr>
const Def* normalize_Shr(const Def*, const Def*, const Def*, const Def*);
template<Wrap>
const Def* normalize_Wrap(const Def*, const Def*, const Def*, const Def*);
template<ROp>
const Def* normalize_ROp(const Def*, const Def*, const Def*, const Def*);
template<ICmp>
const Def* normalize_ICmp(const Def*, const Def*, const Def*, const Def*);
template<RCmp>
const Def* normalize_RCmp(const Def*, const Def*, const Def*, const Def*);
template<Trait>
const Def* normalize_Trait(const Def*, const Def*, const Def*, const Def*);
template<Conv>
const Def* normalize_Conv(const Def*, const Def*, const Def*, const Def*);
template<PE>
const Def* normalize_PE(const Def*, const Def*, const Def*, const Def*);
template<Acc>
const Def* normalize_Acc(const Def*, const Def*, const Def*, const Def*);

namespace normalize {

template<class T>
static T get(u64 u) {
    return bitcast<T>(u);
}

// clang-format off
template<class O> constexpr bool is_int      () { return true;  }
template<>        constexpr bool is_int<ROp >() { return false; }
template<>        constexpr bool is_int<RCmp>() { return false; }
// clang-format on

/*
 * Fold
 */

// This code assumes two-complement arithmetic for unsigned operations.
// This is *implementation-defined* but *NOT* *undefined behavior*.

class Res {
public:
    Res()
        : data_{} {}
    template<class T>
    Res(T val)
        : data_(bitcast<u64>(val)) {}

    constexpr const u64& operator*() const& { return *data_; }
    constexpr u64& operator*() & { return *data_; }
    explicit operator bool() const { return data_.has_value(); }

private:
    std::optional<u64> data_;
};

template<class T, T, nat_t>
struct Fold {};

/*
 * bigger logic used by several ops
 */

template<class T>
constexpr bool is_commutative(T) {
    return false;
}

template<class T>
constexpr bool is_associative(T op) {
    return is_commutative(op);
}

template<class O>
static void commute(O op, const Def*& a, const Def*& b) {
    if (is_commutative(op)) {
        if (b->isa<Lit>() || (a->gid() > b->gid() && !a->isa<Lit>()))
            std::swap(a, b); // swap lit to left, or smaller gid to left if no lit present
    }
}

/// @attention Note that @p a and @p b are passed by reference as fold also commutes if possible. See commute().
template<class Op, Op op, bool isaWrap = std::is_same_v<Op, Wrap>>
static const Def* fold(World& world, const Def* type, const App* callee, const Def*& a, const Def*& b, const Def* dbg) {
    static constexpr int min_w = std::is_same_v<Op, ROp> || std::is_same_v<Op, RCmp> ? 16 : 1;
    auto la = a->isa<Lit>(), lb = b->isa<Lit>();

    if (a->isa<Bot>() || b->isa<Bot>()) return world.bot(type, dbg);

    if (la && lb) {
        nat_t width;
        [[maybe_unused]] bool nsw = false, nuw = false;
        if constexpr (std::is_same_v<Op, Wrap>) {
            auto [mode, w] = callee->args<2>(as_lit<nat_t>);
            nsw            = mode & WMode::nsw;
            nuw            = mode & WMode::nuw;
            width          = w;
        } else if (auto idx = a->type()->isa<Idx>()) {
            width = as_lit(idx->size());
        } else {
            width = as_lit(as<Tag::Real>(a->type())->arg());
        }

        if (is_int<Op>()) width = *size2bitwidth(width);

        Res res;
        switch (width) {
#define CODE(i)                                                             \
    case i:                                                                 \
        if constexpr (i >= min_w) {                                         \
            if constexpr (isaWrap)                                          \
                res = Fold<Op, op, i>::run(la->get(), lb->get(), nsw, nuw); \
            else                                                            \
                res = Fold<Op, op, i>::run(la->get(), lb->get());           \
        }                                                                   \
        break;
            THORIN_1_8_16_32_64(CODE)
#undef CODE
            default: unreachable();
        }

        return res ? world.lit(type, *res, dbg) : world.bot(type, dbg);
    }

    commute(op, a, b);
    return nullptr;
}

} // namespace normalize
} // namespace thorin
