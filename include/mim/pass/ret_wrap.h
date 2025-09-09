#pragma once

#include "mim/pass.h"

namespace mim {

class RetWrap : public RWPass<RetWrap, Lam> {
public:
    RetWrap(World& world, flags_t annex)
        : RWPass(world, annex) {}
    std::unique_ptr<Stage> recreate() final { return std::make_unique<RetWrap>(world(), annex()); }

    void enter() override;
};

} // namespace mim
