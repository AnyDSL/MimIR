#pragma once

#include <thorin/pass/pass.h>

namespace thorin::plug::regex {

class LowerRegex : public RWPass<LowerRegex, Lam> {
public:
    LowerRegex(PassMan& man)
        : RWPass(man, "lower_regex") {}

    Ref rewrite(Ref) override;
};

} // namespace thorin::plug::regex
