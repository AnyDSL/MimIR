#pragma once

#include <mim/def.h>
#include <mim/pass.h>

namespace mim::plug::refly {

/// Removes all `%refly.debug.perm` markers for code gen.
class RemoveDbgPerm : public RWPass<RemoveDbgPerm, Lam> {
public:
    RemoveDbgPerm(World& world, flags_t annex)
        : RWPass(world, annex) {}
    RemoveDbgPerm* recreate() final { return driver().stage<RemoveDbgPerm>(world(), annex()); }

    const Def* rewrite(const Def*) override;
};

} // namespace mim::plug::refly
