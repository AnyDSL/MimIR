#pragma once

#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

/// helper function related to automatic differentiation
namespace thorin::autodiff {

const Def* id_pullback(const Def*);
const Def* zero_pullback(const Def* E, const Def* A);

const Def* tangent_type_fun(const Def*);
const Pi* autodiff_type_fun_pi(const Pi*);
const Def* autodiff_type_fun(const Def*);
const Pi* pullback_type(const Def* E, const Def* A);

const Def* zero_def(const Def* T);
const Def* op_sum(const Def* T, DefArray defs);

} // namespace thorin::autodiff

/// helper functions of general interest
namespace thorin {

// TODO: replace with closedness checks (scopes) at appropriate places
bool is_continuation_type(const Def* E);
bool is_continuation(const Def* e);
// TODO: change name to returning_continuation
bool is_returning_continuation(const Def* e);
bool is_open_continuation(const Def* e);
bool is_direct_style_function(const Def* e);

/// wrapper to access parts of a cps function type
const Def* continuation_dom(const Def* E);
const Def* continuation_codom(const Def* E);

/// computes λ x. f(g(x))
/// the given functions are expected to be in cps
const Def* compose_continuation(const Def* f, const Def* g);

} // namespace thorin

void findAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr);
