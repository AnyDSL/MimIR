#pragma once

#include <mim/phase.h>

namespace mim::plug::refly::phase {

/// Removes all `%refly.debug.perm` markers for code gen.
class RemoveDbg : public Repl {
public:
    RemoveDbg(World& world, flags_t annex)
        : Repl(world, annex) {}

    const Def* replace(const Def*) final;
};

} // namespace mim::plug::refly::phase
