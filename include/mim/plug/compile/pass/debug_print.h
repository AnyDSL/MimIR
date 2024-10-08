#pragma once

#include <mim/def.h>
#include <mim/pass/pass.h>

namespace mim::plug::compile {

class DebugPrint : public RWPass<DebugPrint, Lam> {
public:
    DebugPrint(PassMan& man, int level_)
        : RWPass(man, "debug_print")
        , level(level_) {}

    void enter() override;

private:
    int level;
};

} // namespace mim::plug::compile
