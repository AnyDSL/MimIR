#include "dialects/matrix/passes/lower_matrix_highlevel.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/matrix/matrix.h"
#include "dialects/mem/mem.h"

namespace thorin::matrix {

void findAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr) {
    size_t pos = data.find(toSearch);
    while (pos != std::string::npos) {
        data.replace(pos, toSearch.size(), replaceStr);
        pos = data.find(toSearch, pos + replaceStr.size());
    }
}

const Def* LowerMatrixHighLevelMapRed::rewrite(const Def* def) {
    if (auto i = rewritten.find(def); i != rewritten.end()) return i->second;
    auto new_def   = rewrite_(def);
    rewritten[def] = new_def;
    return rewritten[def];
}

std::optional<const Def*> internal_function_of_axiom(const Axiom* axiom, const Def* meta_args, const Def* args) {
    auto& world      = axiom->world();
    std::string name = axiom->name();
    findAndReplaceAll(name, ".", "_");
    findAndReplaceAll(name, "%", "");
    name = "internal_mapRed_" + name;

    auto replacement = world.lookup(name);
    if (replacement) {
        auto spec_fun = world.app(replacement, meta_args);
        auto ds_fun   = direct::op_cps2ds_dep(spec_fun);
        return world.app(ds_fun, args);
    }
    return std::nullopt;
}

const Def* LowerMatrixHighLevelMapRed::rewrite_(const Def* def) {
    auto& world = def->world();
    // assert(0);

    if (auto mapProd_ax = match<matrix::prod>(def)) {
        world.DLOG("lower product: {}", mapProd_ax);
        auto args      = mapProd_ax->arg();
        auto meta_args = mapProd_ax->callee()->as<App>()->arg();

        auto axiom = mapProd_ax->axiom();

        if (auto internal_fun = internal_function_of_axiom(axiom, meta_args, args)) {
            world.DLOG("  internal_fun: {}", *internal_fun);
            return *internal_fun;
        }
    }

    return def;
}

PassTag* LowerMatrixHighLevelMapRed::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::matrix
