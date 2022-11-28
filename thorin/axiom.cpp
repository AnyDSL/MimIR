#include "thorin/axiom.h"

using namespace std::literals;

namespace thorin {

Axiom::Axiom(NormalizeFn normalizer, const Def* type, dialect_t dialect, tag_t tag, sub_t sub, const Def* dbg)
    : Def(Node, type, Defs{}, dialect | (flags_t(tag) << 8_u64) | flags_t(sub), dbg) {
    u16 curry = 0;
    NomSet done;
    while (auto pi = type->isa<Pi>()) {
        if (auto nom = pi->isa_nom()) {
            if (auto [_, ins] = done.emplace(nom); !ins) break;
        }

        ++curry;
        type = pi->codom();
    }

    normalizer_ = normalizer;
    curry_      = curry;
}

std::optional<dialect_t> Axiom::mangle(std::string_view s) {
    auto n = s.size();
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

std::string Axiom::demangle(dialect_t u) {
    std::string result;
    for (size_t i = 0; i != Max_Dialect_Size; ++i) {
        u64 c = (u & 0xfc00000000000000_u64) >> 58_u64;
        if (c == 0) {
            return result;
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

    return result;
}

static std::string_view sub_view(std::string_view s, size_t i, size_t n = std::string_view::npos) {
    n = std::min(n, s.size());
    return {s.data() + i, n - i};
}

std::optional<std::array<std::string_view, 3>> Axiom::split(std::string_view s) {
    if (s.empty()) return {};
    if (s[0] != '%') return {};
    s = sub_view(s, 1);

    auto dot = s.find('.');
    if (dot == std::string_view::npos) return {};

    auto dialect = sub_view(s, 0, dot);
    if (!mangle(dialect)) return {};

    auto tag = sub_view(s, dot + 1);
    if (auto dot = tag.find('.'); dot != std::string_view::npos) {
        auto sub = sub_view(tag, dot + 1);
        tag      = sub_view(tag, 0, dot);
        return {
            {dialect, tag, sub}
        };
    }

    if (tag.empty()) return {};
    return {
        {dialect, tag, ""sv}
    };
}

std::tuple<const Axiom*, u16> Axiom::get(const Def* def) {
    if (auto axiom = def->isa<Axiom>()) return {axiom, axiom->curry()};
    if (auto app = def->isa<App>()) return {app->axiom(), app->curry()};
    return {nullptr, u16(-1)};
}

} // namespace thorin
