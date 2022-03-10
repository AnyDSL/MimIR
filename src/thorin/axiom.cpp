#include "thorin/axiom.h"

namespace thorin {

Axiom::Axiom(NormalizeFn normalizer, const Def* type, u32 tag, u32 flags, const Def* dbg)
    : Def(Node, type, Defs{}, (nat_t(tag) << 32_u64) | nat_t(flags), dbg) {
    u16 currying_depth = 0;
    while (auto pi = type->isa<Pi>()) {
        ++currying_depth;
        type = pi->codom();
    }

    normalizer_depth_.set(normalizer, currying_depth);
}

std::optional<u64> Axiom::mangle(std::string_view s) {
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

std::string Axiom::demangle(u64 u) {
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

std::tuple<const Axiom*, u16> Axiom::get(const Def* def) {
    if (auto axiom = def->isa<Axiom>()) return {axiom, axiom->currying_depth()};
    if (auto app = def->isa<App>()) return {app->axiom(), app->currying_depth()};
    return {nullptr, u16(-1)};
}

bool is_memop(const Def* def) { return def->isa<App>() && isa<Tag::Mem>(def->proj(0)->type()); }

} // namespace thorin
