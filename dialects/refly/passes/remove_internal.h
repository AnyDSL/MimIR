
#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::refly {

/// Removes all external thorin functions that are marked as internal.
class InternalCleanup : public RWPass<InternalCleanup, Lam> {
public:
    InternalCleanup(PassMan& man)
        : RWPass(man, "internal_cleanup") {}

    void enter() override;
};

} // namespace thorin::refly
