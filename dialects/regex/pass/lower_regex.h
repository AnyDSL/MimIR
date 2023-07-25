#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::regex {

class LowerRegex : public RWPass<LowerRegex, Lam> {
public:
    LowerRegex(PassMan& man)
        : RWPass(man, "lower_regex") {}

    Ref rewrite(Ref) override;
};

} // namespace thorin::regex
