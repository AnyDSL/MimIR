#pragma once

#include "mim/pass/pass.h"

namespace mim {

class RetWrap : public RWPass<RetWrap, Lam> {
public:
    RetWrap(PassMan& man)
        : RWPass(man, "ret_wrap") {}

    void enter() override;
};

} // namespace mim
