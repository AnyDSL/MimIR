#pragma once

#include <mim/def.h>
#include <mim/pass/pass.h>

namespace mim::plug::compile {

class InternalCleanup : public RWPass<InternalCleanup, Lam> {
public:
    InternalCleanup(PassMan& man, const char* prefix = "internal_")
        : RWPass(man, "internal_cleanup")
        , prefix_(prefix) {}

    void enter() override;

private:
    const char* prefix_;
};

} // namespace mim::plug::compile
