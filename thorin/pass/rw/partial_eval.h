#pragma once

#include "thorin/pass/pass.h"

namespace thorin {

class PartialEval : public RWPass<> {
public:
    PartialEval(PassMan& man)
        : RWPass(man, "partial_eval") {}

    const Def* rewrite(const Def*) override;
    static PassTag* ID();
};

} // namespace thorin
