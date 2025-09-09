#pragma once

#include <memory>
#include <mim/def.h>
#include <mim/pass.h>

namespace mim::plug::refly {

/// Removes all `%refly.debug.perm` markers for code gen.
class RemoveDbgPerm : public RWPass<RemoveDbgPerm, Lam> {
public:
    RemoveDbgPerm(World& world, flags_t annex)
        : RWPass(world, annex) {}
    std::unique_ptr<Stage> recreate() final { return std::make_unique<RemoveDbgPerm>(world(), annex()); }

    const Def* rewrite(const Def*) override;
};

} // namespace mim::plug::refly
