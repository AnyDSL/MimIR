#pragma once

#include "thorin/pass/pass.h"

namespace thorin {

class RetWrap : public RWPass<RetWrap, Lam> {
public:
    RetWrap(PassMan& man)
        : RWPass(man, "ret_wrap") {}

    void enter() override;
};

} // namespace thorin
