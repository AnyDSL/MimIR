#include "thorin/axiom.h"

#include "thorin/world.h"

using namespace std::literals;

namespace thorin {

/*
 * Axiom
 */

Axiom::Axiom(NormalizeFn normalizer, u8 curry, u8 trip, const Def* type, plugin_t plugin, tag_t tag, sub_t sub)
    : Def(Node, type, Defs{}, plugin | (flags_t(tag) << 8_u64) | flags_t(sub)) {
    normalizer_ = normalizer;
    curry_      = curry;
    trip_       = trip;
}

std::pair<u8, u8> Axiom::infer_curry_and_trip(const Def* type) {
    u8 curry = 0;
    u8 trip  = 0;
    MutSet done;
    while (auto pi = type->isa<Pi>()) {
        if (auto mut = pi->isa_mut()) {
            if (auto [_, ins] = done.emplace(mut); !ins) {
                // infer trip
                auto curr = pi;
                do {
                    ++trip;
                    curr = curr->codom()->as<Pi>();
                } while (curr != mut);
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

/*
 * Annex
 */

std::optional<plugin_t> Annex::mangle(Sym s) {
    auto n = s->size();
    if (n > Max_Plugin_Size) return {};

    u64 result = 0;
    for (size_t i = 0; i != Max_Plugin_Size; ++i) {
        u64 u = '\0';

        if (i < n) {
            auto c = s[i];
            if (c == '_')
                u = 1;
            else if ('a' <= c && c <= 'z')
                u = c - 'a' + 2_u64;
            else if ('A' <= c && c <= 'Z')
                u = c - 'A' + 28_u64;
            else if ('0' <= c && c <= '9')
                u = c - '0' + 54_u64;
            else
                return {};
        }

        result = (result << 6_u64) | u;
    }

    return result << 16_u64;
}

Sym Annex::demangle(World& world, plugin_t u) {
    std::string result;
    for (size_t i = 0; i != Max_Plugin_Size; ++i) {
        u64 c = (u & 0xfc00000000000000_u64) >> 58_u64;
        if (c == 0)
            return world.sym(result);
        else if (c == 1)
            result += '_';
        else if (2 <= c && c < 28)
            result += 'a' + ((char)c - 2);
        else if (28 <= c && c < 54)
            result += 'A' + ((char)c - 28);
        else
            result += '0' + ((char)c - 54);

        u <<= 6_u64;
    }

    return world.sym(result);
}

std::array<Sym, 3> Annex::split(World& world, Sym s) {
    if (!s) return {};
    if (s[0] != '%') return {};
    auto sv = subview(s, 1);

    auto dot = sv.find('.');
    if (dot == std::string_view::npos) return {};

    auto plugin = world.sym(subview(sv, 0, dot));
    if (!mangle(plugin)) return {};

    auto tag = subview(sv, dot + 1);
    if (auto dot = tag.find('.'); dot != std::string_view::npos) {
        auto sub = world.sym(subview(tag, dot + 1));
        tag      = subview(tag, 0, dot);
        return {plugin, world.sym(tag), sub};
    }

    return {plugin, world.sym(tag), {}};
}

} // namespace thorin
