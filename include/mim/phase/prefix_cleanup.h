#pragma once

#include "mim/phase/phase.h"

namespace mim {

class PrefixCleanup : public RWPhase {
public:
    PrefixCleanup(World&, std::string prefix = "internal_");

private:
    void rewrite_externals() final;

    std::string prefix_;
};

} // namespace mim
