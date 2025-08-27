#pragma once

#include <mim/world.h>

#include "mim/plug/compile/autogen.h"

namespace mim::plug::compile {

template<class P, class B>
void apply(P& ps, B& builder, const Def* app) {
    auto& world     = app->world();
    auto [p_def, _] = App::uncurry(app);

    world.DLOG("pass/phase: {}", p_def);

    if (auto phase_ax = p_def->isa<Axm>()) {
        auto flag = phase_ax->flags();
        if (ps.contains(flag)) {
            auto phase_fun = ps[flag];
            phase_fun(builder, app);
        } else {
            world.WLOG("pass/phase '{}' not found", phase_ax->sym());
            assert(ps.contains(flag) && "pass/phase not found");
        }
    } else if (p_def->isa<Lam>()) {
        assert(0 && "curried lambas are not supported");
    } else {
        world.WLOG("pass/phase '{}' is not an axm", p_def);
        assert(p_def->isa<Axm>() && "pass/phase is not an axm");
    }
}

} // namespace mim::plug::compile
