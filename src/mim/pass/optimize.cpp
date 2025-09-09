#include "mim/pass/optimize.h"

#include "mim/driver.h"
#include "mim/phase.h"

namespace mim {

void optimize(World& world) {
    // clang-format off
    auto compilation_functions = {
        world.sym("_compile"),
        world.sym("_default_compile"),
        world.sym("_core_compile"),
        world.sym("_fallback_compile")
    };
    // clang-format on

    const Def* compilation = nullptr;
    for (auto compilation_function : compilation_functions) {
        if (auto compilation_ = world.external(compilation_function)) {
            if (!compilation) compilation = compilation_;
            compilation_->make_internal();
        }
    }

    // make all functions `[] -> %compile.Phase` internal
    for (auto def : world.copy_externals()) {
        if (auto lam = def->isa<Lam>(); lam && lam->num_doms() == 0) {
            // TODO use Axm::isa - but rn there is a problem with the rec Pi and plugin deps
            if (lam->codom()->sym().view() == "%compile.Phase") {
                if (!compilation) compilation = lam;
                def->make_internal();
            }
        }
    }

    if (!compilation) world.ELOG("no compilation function found");
    world.DLOG("compilation using {} : {}", compilation, compilation->type());

    auto body  = compilation->as<Lam>()->body();
    auto phase = Phase::make_unique(body);
    phase->run();
}

} // namespace mim
