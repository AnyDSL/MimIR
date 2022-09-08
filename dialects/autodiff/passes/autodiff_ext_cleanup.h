#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::autodiff {

class AutoDiffExternalCleanup : public RWPass<AutoDiffExternalCleanup, Lam> {
public:
    AutoDiffExternalCleanup(PassMan& man)
        : RWPass(man, "autodiff_external_cleanup") {}

    void enter() override;
};

} // namespace thorin::autodiff
