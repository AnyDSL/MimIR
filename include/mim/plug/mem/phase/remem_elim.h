#pragma once

#include <mim/phase.h>

namespace mim::plug::mem::phase {

class RememElim : public Repl {
public:
    RememElim(World& world, flags_t annex)
        : Repl(world, annex) {}

    const Def* replace(const Def*) final;
};

} // namespace mim::plug::mem::phase
