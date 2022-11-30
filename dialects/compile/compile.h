#pragma once

#include <thorin/world.h>

#include "thorin/pass/pipelinebuilder.h"

#include "dialects/compile/autogen.h"

namespace thorin::compile {
inline void handle_optimization_part(const Def* part, World& world, Passes& passes, PipelineBuilder& builder) {
    auto [phase_def, phase_args] = collect_args(part);
    world.DLOG("pass/phase: {}", phase_def);
    if (auto phase_ax = phase_def->isa<Axiom>()) {
        auto flag = phase_ax->flags();
        if (passes.contains(flag)) {
            auto phase_fun = passes[flag];
            phase_fun(world, builder, part);
        } else {
            world.WLOG("pass/phase '{}' not found", phase_ax->name());
            assert(passes.contains(flag) && "pass/phase not found");
        }
    } else {
        world.WLOG("pass/phase '{}' is not an axiom", phase_def);
    }
}
} // namespace thorin::compile
