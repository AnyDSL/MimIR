#pragma once

#include "mim/phase.h"

namespace mim {

class PrefixCleanup : public RWPhase {
public:
    PrefixCleanup(World& world, flags_t annex, std::string prefix);

    PrefixCleanup* recreate() final { return driver().stage<PrefixCleanup>(old_world(), annex(), prefix_); }

private:
    void rewrite_external(Def*) final;

    std::string prefix_;
};

} // namespace mim
