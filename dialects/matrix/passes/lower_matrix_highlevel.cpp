#include "dialects/matrix/passes/lower_matrix_highlevel.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/matrix/matrix.h"
#include "dialects/mem/mem.h"

namespace thorin::matrix {

const Def* LowerMatrixHighLevel::rewrite(const Def* def) {
    if (auto i = rewritten.find(def); i != rewritten.end()) return i->second;
    rewritten[def] = rewrite_(def);
    return rewritten[def];
}

const Def* LowerMatrixHighLevel::rewrite_(const Def* def) {
    auto& world = def->world();

    // if (auto mapReduce_ax = match<matrix::mapReduce>(def); mapReduce_ax) {}

    return def;
}

PassTag* LowerMatrixHighLevel::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::matrix
