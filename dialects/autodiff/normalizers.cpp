#include <iostream>

#include "thorin/axiom.h"
#include "thorin/world.h"

#include "dialects/autodiff/analysis/helper.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/core/core.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

const Def* normalize_autodiff_type(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    // return arg;

    auto result = autodiff_type_fun(arg);
    if (result != nullptr) { return result; }
    return world.raw_app(callee, arg, dbg);
}

const Def* normalize_tangent_type(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();
    return tangent_type_fun(arg);
}

THORIN_autodiff_NORMALIZER_IMPL

} // namespace thorin::autodiff
