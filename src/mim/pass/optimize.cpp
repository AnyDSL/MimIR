#include "mim/pass/optimize.h"

#include "mim/driver.h"

#include "mim/phase/phase.h"

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

    // make all functions `[] -> Pipeline` internal
    for (auto def : world.copy_externals()) {
        if (auto lam = def->isa<Lam>(); lam && lam->num_doms() == 0) {
            if (lam->codom()->sym().view() == "%compile.Pipeline") {
                if (!compilation) compilation = lam;
                def->make_internal();
            }
        }
    }

    if (!compilation) world.ELOG("no compilation function found");
    world.DLOG("compilation using {} : {}", compilation, compilation->type());

    // We can not directly access compile axms here.
    // But the compile plugin has not the necessary communication pipeline.
    // Therefore, we register the handlers and let the compile plugin call them.

    auto pipe             = Pipeline(world);
    auto pipeline_prog    = compilation->as<Lam>()->body();
    auto [callee, phases] = App::uncurry(pipeline_prog);
    auto axm              = callee->as<Axm>();

    world.DLOG("Building pipeline");
    if (auto phase = world.driver().phase(axm->flags()))
        (*phase)(pipe, pipeline_prog);
    else
        world.ELOG("axm not found in passes");

    pipe.run();
}

} // namespace mim
