#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::compile {

class InternalCleanup : public RWPass<InternalCleanup, Lam> {
public:
    InternalCleanup(PassMan& man)
        : RWPass(man, "internal_cleanup") {}

    void enter() override;
};

} // namespace thorin::compile
