#include "dialects/matrix/passes/lower_matrix_lowlevel.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/matrix/matrix.h"
#include "dialects/mem/mem.h"

namespace thorin::matrix {

const Def* LowerMatrixLowLevel::rewrite(const Def* def) {
    if (auto i = rewritten.find(def); i != rewritten.end()) return i->second;
    rewritten[def] = rewrite_(def);
    return rewritten[def];
}

const Def* LowerMatrixLowLevel::rewrite_(const Def* def) {
    auto& world = def->world();

    assert(!match<matrix::mapReduce>(def) && "mapReduce should have been lowered to for loops by now");
    if (auto mat_ax = match<matrix::Mat>(def)) {
        auto [n_def, S, T] = mat_ax->args<3>();
        world.DLOG("Lowering Mat to Ptr");
        auto n = (size_t)(n_def->as<Lit>()->get<u64>());
        for (size_t i = 0; i < n; i++) {
            // auto ptr = world.
            // rewritten[mat_ax->arg(i)] = ptr;
        }
    }

    return def;
}

PassTag* LowerMatrixLowLevel::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::matrix
