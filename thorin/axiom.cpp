#include "thorin/axiom.h"

#include "thorin/world.h"

using namespace std::literals;

namespace thorin {

Axiom::Axiom(NormalizeFn normalizer, u8 curry, u8 trip, const Def* type, dialect_t dialect, tag_t tag, sub_t sub)
    : Def(Node, type, Defs{}, dialect | (flags_t(tag) << 8_u64) | flags_t(sub)) {
    normalizer_ = normalizer;
    curry_      = curry;
    trip_       = trip;
}

std::pair<u8, u8> Axiom::infer_curry_and_trip(const Def* type) {
    u8 curry = 0;
    u8 trip  = 0;
    NomSet done;
    while (auto pi = type->isa<Pi>()) {
        if (auto nom = pi->isa_nom()) {
            if (auto [_, ins] = done.emplace(nom); !ins) {
                // infer trip
                auto curr = pi;
                do {
                    ++trip;
                    curr = curr->codom()->as<Pi>();
                } while (curr != nom);
                break;
            }
        }

        ++curry;
        type = pi->codom();
    }

    return {curry, trip};
}

std::tuple<const Axiom*, u8, u8> Axiom::get(const Def* def) {
    if (auto axiom = def->isa<Axiom>()) return {axiom, axiom->curry(), axiom->trip()};
    if (auto app = def->isa<App>()) return {app->axiom(), app->curry(), app->trip()};
    return {nullptr, 0, 0};
}

std::optional<dialect_t> Axiom::mangle(Sym s) {
    auto n = s->size();
    if (n > Max_Dialect_Size) return {};

    u64 result = 0;
    for (size_t i = 0; i != Max_Dialect_Size; ++i) {
        u64 u = '\0';

        if (i < n) {
            auto c = s[i];
            if (c == '_') {
                u = 1;
            } else if ('a' <= c && c <= 'z') {
                u = c - 'a' + 2_u64;
            } else if ('A' <= c && c <= 'Z') {
                u = c - 'A' + 28_u64;
            } else if ('0' <= c && c <= '9') {
                u = c - '0' + 54_u64;
            } else {
                return {};
            }
        }

        result = (result << 6_u64) | u;
    }

    return result << 16_u64;
}

Sym Axiom::demangle(World& world, dialect_t u) {
    std::string result;
    for (size_t i = 0; i != Max_Dialect_Size; ++i) {
        u64 c = (u & 0xfc00000000000000_u64) >> 58_u64;
        if (c == 0) {
            return world.sym(result);
        } else if (c == 1) {
            result += '_';
        } else if (2 <= c && c < 28) {
            result += 'a' + ((char)c - 2);
        } else if (28 <= c && c < 54) {
            result += 'A' + ((char)c - 28);
        } else {
            result += '0' + ((char)c - 54);
        }

        u <<= 6_u64;
    }

    return world.sym(result);
}

std::array<Sym, 3> Axiom::split(World& world, Sym s) {
    if (!s) return {};
    if (s[0] != '%') return {};
    auto sv = subview(s, 1);

    auto dot = sv.find('.');
    if (dot == std::string_view::npos) return {};

    auto dialect = world.sym(subview(sv, 0, dot));
    if (!mangle(dialect)) return {};

    auto tag = subview(sv, dot + 1);
    if (auto dot = tag.find('.'); dot != std::string_view::npos) {
        auto sub = world.sym(subview(tag, dot + 1));
        tag      = subview(tag, 0, dot);
        return {dialect, world.sym(tag), sub};
    }

    return {dialect, world.sym(tag), {}};
}

} // namespace thorin
