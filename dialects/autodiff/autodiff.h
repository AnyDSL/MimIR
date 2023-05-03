#pragma once

#include "thorin/world.h"

#include "dialects/autodiff/autogen.h"

namespace thorin::autodiff {

/// @name %%autodiff.Tangent
///@{
const Def* tangent_type_fun(const Def*);
///@}

/// @name %%autodiff.zero
///@{
const Def* zero_def(const Def* T);
const Def* zero_pullback(const Def* E, const Def* A);
///@}

/// @name %%autodiff.sum
///@{
const Def* op_sum(const Def* T, DefArray defs);
///@}

/// @name Helpers
///@{
const Pi* pullback_type(const Def* E, const Def* A);
const Def* id_pullback(const Def*);
const Pi* autodiff_type_fun_pi(const Pi*);
const Def* autodiff_type_fun(const Def*);
///@}

} // namespace thorin::autodiff
