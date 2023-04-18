#include "dialects/matrix/passes/lower_matrix_highlevel.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/core/core.h"
#include "dialects/direct/direct.h"
#include "dialects/matrix/matrix.h"
#include "dialects/mem/mem.h"

namespace thorin::matrix {

Ref LowerMatrixHighLevelMapRed::rewrite(Ref def) {
    if (auto i = rewritten.find(def); i != rewritten.end()) return i->second;
    auto new_def   = rewrite_(def);
    rewritten[def] = new_def;
    return rewritten[def];
}

std::optional<Ref> internal_function_of_axiom(const Axiom* axiom, Ref meta_args, Ref args) {
    auto& world      = axiom->world();
    std::string name = *axiom->sym();
    find_and_replace(name, ".", "_");
    find_and_replace(name, "%", "");
    name = INTERNAL_PREFIX + name;

    auto replacement = world.lookup(world.sym(name));
    if (replacement) {
        auto spec_fun = world.app(replacement, meta_args);
        auto ds_fun   = direct::op_cps2ds_dep(spec_fun);
        return world.app(ds_fun, args);
    }
    return std::nullopt;
}

Ref LowerMatrixHighLevelMapRed::rewrite_(Ref def) {
    auto& world = def->world();

    if (auto mat_ax = match<matrix::prod>(def)) {
        auto args      = mat_ax->arg();
        auto meta_args = mat_ax->callee()->as<App>()->arg();

        auto [m, k, l, w] = meta_args->projs<4>();
        auto [mem, M, N]  = args->projs<3>();

        auto w_lit = w->isa<Lit>();

        auto ext_fun = world.lookup(world.sym("extern_matrix_prod"));
        if (ext_fun && (w_lit && w_lit->get<u64>() == 64)) {
            auto ds_fun  = direct::op_cps2ds_dep(ext_fun);
            auto fun_app = world.app(ds_fun, {mem, m, k, l, M, N});
            return fun_app;
        }
    }

    if (auto outer_app = def->isa<App>()) {
        if (auto inner_app = outer_app->callee()->isa<App>()) {
            if (auto axiom = inner_app->callee()->isa<Axiom>()) {
                if (auto internal_function = internal_function_of_axiom(axiom, inner_app->arg(), outer_app->arg())) {
                    world.DLOG("lower matrix axiom {} in {} : {}", *axiom->sym(), def, def->type());
                    world.DLOG("lower matrix axiom using: {} : {}", *internal_function, (*internal_function)->type());
                    return *internal_function;
                }
            }
        }
    }

    return def;
}

} // namespace thorin::matrix
