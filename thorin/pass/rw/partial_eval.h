#pragma once

#include "thorin/pass/pass.h"

namespace thorin {

class PartialEval : public RWPass<PartialEval, Def> {
public:
    PartialEval(PassMan& man)
        : RWPass(man, "partial_eval") {}

    Ref rewrite(Ref) override;
};

} // namespace thorin
