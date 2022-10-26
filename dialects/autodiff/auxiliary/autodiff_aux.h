#pragma once

#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

namespace thorin::autodiff {

const Def* id_pullback(const Def*);
const Def* zero_pullback(const Def* E, const Def* A);

const Def* tangent_type_fun(const Def*);
const Def* autodiff_inner_type_fun(const Def* B, const Def* A);
const Pi* autodiff_type_fun_pi(const Pi*);
const Def* autodiff_type_fun(const Def*);
const Pi* pullback_type(const Def* E, const Def* A);

const Def* zero_def(const Def* T);
const Def* op_sum(const Def* T, DefArray defs);

} // namespace thorin::autodiff

namespace thorin {

bool is_closed(Lam* lam);

// TODO: replace with closedness checks (scopes) at appropriate places
bool is_continuation_type(const Def* E);
bool is_continuation(const Def* e);
// TODO: change name to returning_continuation
bool is_returning_continuation(const Def* e);
bool is_returning_continuation_type(const Def* E);
bool is_open_continuation(const Def* e);
bool is_direct_style_function(const Def* e);

const Def* continuation_dom(const Def* E);
const Def* continuation_codom(const Def* E);

/// Computes the composition `λ x. f(g(x))`.
/// The given functions `f` and `g` are expected to be in cps.
const Def* compose_continuation(const Def* f, const Def* g);

//// memory operations

/// Adds memory types to a function type (and its return type).
const Pi* cn_mem_wrap(const Pi* pi);
/// Returns whether the type T contains a memory type.
bool contains_mem(const Def* T);
/// Adds a memory type to a sigma/pack/array as flat tuple.
const Def* equip_mem(const Def* T);
const Def* lam_mem_wrap(const Def* lam);

} // namespace thorin

/// General thorin-unrelated C++ helper functions.
void findAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr);
