#include <iostream>

#include <thorin/lam.h>
#include <thorin/tables.h>

#include "dialects/affine/affine.h"
#include "dialects/core/core.h"
// #include "dialects/direct/direct.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

const Def* AutoDiffEval::rewrite(const Def* def) {
    if (auto i = rewritten.find(def); i != rewritten.end()) return i->second;
    rewritten[def] = rewrite_(def);
    return rewritten[def];
}

const Def* AutoDiffEval::rewrite_(const Def* def) {
    auto& world = def->world();

    if (auto ad_app = match<autodiff>(def); ad_app) {
        world.DLOG("found a autodiff::autodiff");
        return def;
    }

    return def;
}

PassTag* AutoDiffEval::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::autodiff
