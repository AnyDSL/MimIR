#pragma once

#include <thorin/world.h>

#include "thorin/pass/pipelinebuilder.h"

#include "dialects/compile/autogen.h"

namespace thorin::compile {
inline void handle_optimization_part(const Def* part, World& world, Passes& passes, PipelineBuilder& builder) {
    if (auto app = part->isa<App>()) {
        if (auto lam = app->callee()->isa<Lam>()) {
            part = lam->reduce(app->arg())[1];
            world.DLOG("reduce pass/phase lambda {} to {} : {}", lam, part, part->type());
        }
    }

    auto [phase_def, phase_args] = collect_args(part);
    world.DLOG("pass/phase: {}", phase_def);
    if (auto phase_ax = phase_def->isa<Axiom>()) {
        auto flag = phase_ax->flags();
        if (passes.contains(flag)) {
            auto phase_fun = passes[flag];
            phase_fun(world, builder, part);
        } else {
            world.WLOG("pass/phase '{}' not found", phase_ax->sym());
            assert(passes.contains(flag) && "pass/phase not found");
        }
    } else if (phase_def->isa<Lam>()) {
        assert(0 && "curried lambas are not supported");
    } else {
        world.WLOG("pass/phase '{}' is not an axiom", phase_def);
        assert(phase_def->isa<Axiom>() && "pass/phase is not an axiom");
    }
}
} // namespace thorin::compile
