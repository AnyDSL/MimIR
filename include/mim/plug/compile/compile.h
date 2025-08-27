#pragma once

#include <mim/world.h>

#include "mim/plug/compile/autogen.h"

namespace mim::plug::compile {

template<class P, class B>
void apply(P& ps, B& builder, const Def* app) {
    auto& world     = app->world();
    auto [p_def, _] = App::uncurry(app);

    world.DLOG("pass/phase: {}", p_def);

    if (auto axm = p_def->isa<Axm>())
        if (auto i = ps.find(axm->flags()); i != ps.end())
            i->second(builder, app);
        else
            world.ELOG("pass/phase '{}' not found", axm->sym());
    else
        world.ELOG("unsupported callee for a a phase/pass", p_def);
}

} // namespace mim::plug::compile
