#pragma once

#include <thorin/def.h>

#include <thorin/pass/pass.h>

namespace thorin::plug::compile {

class DebugPrint : public RWPass<DebugPrint, Lam> {
public:
    DebugPrint(PassMan& man, int level_)
        : RWPass(man, "debug_print")
        , level(level_) {}

    void enter() override;

private:
    int level;
};

} // namespace thorin::plug::compile
