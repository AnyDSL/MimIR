#pragma once

#include "mim/phase/phase.h"

namespace mim {

class PrefixCleanup : public RWPhase {
public:
    PrefixCleanup(World&, std::string prefix = "internal_");

private:
    void start() final;

    std::string prefix_;
};

} // namespace mim
