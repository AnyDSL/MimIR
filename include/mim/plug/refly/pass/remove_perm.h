#pragma once

#include <mim/def.h>
#include <mim/pass/pass.h>

namespace mim::plug::refly {

/// Removes all `%refly.debug.perm` markers for code gen.
class RemoveDbgPerm : public RWPass<RemoveDbgPerm, Lam> {
public:
    RemoveDbgPerm(PassMan& man)
        : RWPass(man, "remove_dbg_perm") {}

    Ref rewrite(Ref) override;
};

} // namespace mim::plug::refly
