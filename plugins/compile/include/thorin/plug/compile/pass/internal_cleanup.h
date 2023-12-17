#pragma once

#include <thorin/def.h>

#include <thorin/pass/pass.h>

namespace thorin::plug::compile {

class InternalCleanup : public RWPass<InternalCleanup, Lam> {
public:
    InternalCleanup(PassMan& man, const char* prefix = "internal_")
        : RWPass(man, "internal_cleanup")
        , prefix_(prefix) {}

    void enter() override;

private:
    const char* prefix_;
};

} // namespace thorin::plug::compile
