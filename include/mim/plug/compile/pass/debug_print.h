#pragma once

#include <mim/def.h>
#include <mim/pass.h>

namespace mim::plug::compile {

class DebugPrint : public RWPass<DebugPrint, Lam> {
public:
    DebugPrint(PassMan& man, nat_t level)
        : RWPass(man, "debug_print")
        , level_(level) {}

    void enter() override;

private:
    int level_;
};

} // namespace mim::plug::compile
