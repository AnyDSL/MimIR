#pragma once

#include <thorin/config.h>
#include <thorin/pass/pass.h>

using namespace thorin;

/// A pass that just dumps the world.
class DebugDump : public RWPass<DebugDump, Lam> {
public:
    DebugDump(PassMan& man)
        : RWPass(man, "print_wrapper") {}

    void prepare() override { world().debug_dump(); }
};
