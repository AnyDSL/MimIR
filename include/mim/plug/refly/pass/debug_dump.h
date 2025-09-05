#pragma once

#include <mim/config.h>
#include <mim/pass.h>

using namespace mim;

/// A pass that just dumps the world.
class DebugDump : public RWPass<DebugDump, Lam> {
public:
    DebugDump(PassMan& man)
        : RWPass(man, "print_wrapper") {}

    void prepare() override { world().debug_dump(); }
};
