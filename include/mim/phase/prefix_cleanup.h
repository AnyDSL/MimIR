#pragma once

#include "mim/phase.h"

namespace mim {

class PrefixCleanup : public RWPhase {
public:
    PrefixCleanup(World&, std::string prefix = "internal_");

private:
    void rewrite_external(Def*) final;

    std::string prefix_;
};

} // namespace mim
