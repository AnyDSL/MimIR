#pragma once

#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

namespace thorin::autodiff {

const Def* id_pullback(const Def*);
const Def* zero_pullback(const Def* E, const Def* A);

const Def* tangent_type_fun(const Def*);
// R const Def* augment_type_fun(const Def*);
const Pi* autodiff_type_fun_pi(const Pi*);
const Def* autodiff_type_fun(const Def*);
const Pi* pullback_type(const Def* E, const Def* A);

const Def* zero_def(const Def* T);
const Def* op_sum(const Def* T, DefArray defs);

} // namespace thorin::autodiff

namespace thorin {

bool is_continuation_type(const Def* E);
bool is_continuation(const Def* e);
// TODO: change name to returning_continuation
bool is_returning_continuation(const Def* e);
bool is_open_continuation(const Def* e);
bool is_direct_style_function(const Def* e);

const Def* continuation_dom(const Def* E);
const Def* continuation_codom(const Def* E);

/// computes λ x. f(g(x))
/// the given functions are expected to be in cps
const Def* compose_continuation(const Def* f, const Def* g);

// /// match without curry check
// template<class AxTag, bool Check = true>
// Match<AxTag, detail::Enum2Def<AxTag>> raw_match(const Def* def) {
//     auto [axiom, curry] = Axiom::get(def);
//     if constexpr (Check) {
//         if (axiom && (axiom->flags() & ~0xFF_u64) == detail::base_value<AxTag>())
//             return {axiom, def->as<detail::Enum2Def<AxTag>>()};
//         return {};
//     }
//     assert(axiom && (axiom->flags() & ~0xFF_u64) == detail::base_value<AxTag>() && "assumed to be correct axiom");
//     return {axiom, def->as<detail::Enum2Def<AxTag>>()};
// }

// // equivalent to flags == given axiom
// // but error with cast
// template<class AxTag, bool Check = true>
// Match<AxTag, detail::Enum2Def<AxTag>> raw_match(AxTag sub, const Def* def) {
//     auto [axiom, curry] = Axiom::get(def);
//     if constexpr (Check) {
//         if (axiom && axiom->flags() == sub) return {axiom, def->as<detail::Enum2Def<AxTag>>()};
//         return {};
//     }
//     assert(axiom && axiom->flags() == sub && "assumed to be correct axiom");
//     return {axiom, def->as<detail::Enum2Def<AxTag>>()};
// }

} // namespace thorin

void findAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr);