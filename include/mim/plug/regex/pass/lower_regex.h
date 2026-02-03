#pragma once

#include <mim/pass.h>

namespace mim::plug::regex {

class LowerRegex : public RWPass<LowerRegex, Lam> {
public:
    LowerRegex(World& world, flags_t annex)
        : RWPass(world, annex) {}

    const Def* rewrite(const Def*) override;
};

} // namespace mim::plug::regex
