#pragma once

#include <thorin/def.h>

#include <thorin/pass/pass.h>

namespace thorin::plug::refly {

/// Removes all `%refly.debug.perm` markers for code gen.
class RemoveDbgPerm : public RWPass<RemoveDbgPerm, Lam> {
public:
    RemoveDbgPerm(PassMan& man)
        : RWPass(man, "remove_dbg_perm") {}

    Ref rewrite(Ref) override;
};

} // namespace thorin::plug::refly
