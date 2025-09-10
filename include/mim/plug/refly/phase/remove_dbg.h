#pragma once

#include <mim/def.h>
#include <mim/phase.h>

namespace mim::plug::refly::phase {

/// Removes all `%refly.debug.perm` markers for code gen.
class RemoveDbg : public RWPhase {
public:
    RemoveDbg(World& world, flags_t annex)
        : RWPhase(world, annex) {}

    const Def* rewrite(const Def*) override;
};

} // namespace mim::plug::refly::phase
