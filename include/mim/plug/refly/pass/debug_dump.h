#pragma once

#include <mim/config.h>
#include <mim/pass.h>

using namespace mim;

/// A pass that just dumps the world.
class DebugDump : public RWPass<DebugDump, Lam> {
public:
    DebugDump(World& world, flags_t annex)
        : RWPass(world, annex) {}

    void prepare() override { world().debug_dump(); }
};
