#pragma once

#include "mim/phase/phase.h"

namespace mim {

/// Inlines in post-order all Lam%s that occur exactly *once* in the program.
class EtaRedPhase : public RWPhase {
public:
    EtaRedPhase(World& world)
        : RWPhase(world, "eta reduction") {}

private:
    void start() final;
    const Def* rewrite_mut_Lam(Lam*) final;
};

} // namespace mim
