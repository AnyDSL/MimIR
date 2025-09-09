#pragma once

#include "mim/phase.h"

namespace mim {

class PrefixCleanup : public RWPhase {
public:
    PrefixCleanup(World& world, flags_t annex, std::string prefix);

    std::unique_ptr<Stage> recreate() final { return std::make_unique<PrefixCleanup>(old_world(), annex(), prefix_); }

private:
    void rewrite_external(Def*) final;

    std::string prefix_;
};

} // namespace mim
