#include "dialects/mem/passes/rw/add_mem.h"

#include <cassert>

#include <iostream>

#include <thorin/lam.h>

#include "thorin/axiom.h"
#include "thorin/def.h"

#include "dialects/core/core.h"
#include "dialects/mem/mem.h"

namespace thorin::mem {

bool containsMem(const Def* A) {
    if (match<mem::M>(A)) { return true; }

    if (A->num_projs() > 1) {
        for (auto proj : A->projs()) {
            if (containsMem(proj)) { return true; }
        }
    }

    return false;
}

// const Pi* add_mem(const Pi* pi) { }

const Def* AddMem::rewrite_structural(const Def* def) {
    auto& world = def->world();

    if (auto pi = def->isa<Pi>()) {
        world.DLOG("rewrite Pi {}", pi);
        if (pi->codom()->isa<Bot>()) {
            // world.DLOG("rewrite_structural: {} : {}", def, def->type());
            if (!containsMem(pi->dom())) {
                // currently adds mem in front (flat): [..dom] -> [mem, ..dom]
                auto aug_dom = rewrite(pi->dom());

                world.DLOG("Add mem to : {} : {}", def, def->type());
                world.DLOG("Aug Dom : {} : {}", aug_dom, aug_dom->type());
                DefArray types(aug_dom->num_projs() + 1, [&](size_t i) {
                    return i == 0 ? world.ax<M>() : aug_dom->proj(aug_dom->num_projs(), i - 1);
                });
                auto new_dom  = world.sigma(types);
                auto new_type = world.pi(new_dom, pi->codom());
                world.DLOG("Result : {} : {}", new_type, new_type->type());
                return new_type;
            }
        }

        // auto mem = world.var(type_mem(world));
        // auto new_domain = world.sigma({mem, pi->domain()});
        // auto new_codomain = world.sigma({mem, pi->codomain()});
        // return world.pi(new_domain, new_codomain);
    }

    if (auto app = def->isa<App>()) {
        auto callee     = app->callee();
        auto arg        = app->arg();
        auto new_callee = rewrite(callee);
        auto new_arg    = rewrite(arg);

        auto mem_arg = mem_def(arg);
        // TODO: handle cascading (at this point cascading should not occur)
        // TODO: handle if

        world.DLOG("app : {} : {}", def, def->type());
        world.DLOG("  callee : {} : {} [{}]", new_callee, new_callee->type(), new_callee->node_name());
        world.DLOG("  arg : {} : {}", new_arg, new_arg->type());
        if (!mem_arg &&
            // !Axiom, !App, !Idx
            (new_callee->isa<Lam>() || new_callee->isa<Var>() || new_callee->isa<Extract>())) {
            world.DLOG("non-mem app ");
            // TODO: keep non flat shape?
            assert(current_mem);
            DefArray mem_args(new_arg->num_projs() + 1, [&](size_t i) {
                return i == 0 ? current_mem : new_arg->proj(new_arg->num_projs(), i - 1);
            });
            new_arg = world.tuple(mem_args);
        }
        // if (auto mem = mem_def(arg)) {
        //     world.DLOG("mem app : {} : {}", def, def->type());
        // } else {
        //     world.DLOG("non-mem app : {} : {}", def, def->type());
        // }
        return world.app(new_callee, new_arg);
    }

    return Rewriter::rewrite_structural(def); // continue recursive rewriting with everything else
    // return def;
}

const Def* AddMem::rewrite_nom(Def* def) {
    auto& world = def->world();

    // auto new_def = Rewriter::rewrite_nom(def); // continue recursive rewriting with everything else

    // TODO: check why rewrite does not work
    auto new_type = rewrite_structural(def->type());
    auto new_dbg  = rewrite(def->dbg());
    auto new_nom  = def->stub(world, new_type, new_dbg);
    map(def, new_nom);

    if (new_nom->isa<Lam>()) {
        world.DLOG("enter Lam {} : {}", new_nom, new_nom->type());
        world.DLOG("  type {} => {}", def->type(), new_type);
        current_mem = mem_def(new_nom->var());
    }

    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        if (auto old_op = def->op(i)) new_nom->set(i, rewrite(old_op));
    }

    if (auto new_def = new_nom->restructure()) return map(def, new_def);
    return new_nom;

    // world.DLOG("rewrite_nom: {} : {}", def, def->type());
    // world.DLOG("  to: {} : {}", new_def, new_def->type());
    // return new_def;
}

} // namespace thorin::mem
