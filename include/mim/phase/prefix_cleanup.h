#pragma once

#include "mim/phase.h"

namespace mim {

class PrefixCleanup : public RWPhase {
public:
    PrefixCleanup(World& world, flags_t annex)
        : RWPhase(world, annex) {}

    void apply(std::string);
    void apply(const App*) final;
    void apply(Phase&) final;

private:
    void rewrite_external(Def*) final;

    std::string prefix_;
};

} // namespace mim
