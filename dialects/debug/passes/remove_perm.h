#pragma once

#include <thorin/def.h>
#include <thorin/pass/pass.h>

namespace thorin::debug {

/// removes all permanent debug markers for code gen
class DebugRemovePerm : public RWPass<DebugRemovePerm, Lam> {
public:
    DebugRemovePerm(PassMan& man)
        : RWPass(man, "debug_remove_perm") {}

    const Def* rewrite(const Def*) override;
};

} // namespace thorin::debug
