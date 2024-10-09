#pragma once

#include <mim/pass/pass.h>

namespace mim::plug::regex {

class LowerRegex : public RWPass<LowerRegex, Lam> {
public:
    LowerRegex(PassMan& man)
        : RWPass(man, "lower_regex") {}

    Ref rewrite(Ref) override;
};

} // namespace mim::plug::regex
