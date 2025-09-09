#pragma once

#include "mim/pass.h"

namespace mim {

class RetWrap : public RWPass<RetWrap, Lam> {
public:
    RetWrap(World& world, flags_t annex)
        : RWPass(world, annex) {}
    RetWrap* recreate() final { return driver().stage<RetWrap>(world(), annex()); }

    void enter() override;
};

} // namespace mim
