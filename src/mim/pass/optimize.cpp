#include "mim/pass/optimize.h"

#include "mim/driver.h"

#include "mim/pass/pipelinebuilder.h"
#include "mim/phase/phase.h"

namespace mim {

void optimize(World& world) {
    auto compilation_functions = {world.sym("_compile"), world.sym("_default_compile"), world.sym("_core_compile"),
                                  world.sym("_fallback_compile")};
    const Def* compilation     = nullptr;
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
    assert(compilation && "no compilation function found");

    // We found a compilation directive in the file and use it to build the compilation pipeline.
    // The general idea is that passes and phases are exposed as axms.
    // Each pass/phase axm is associated with a handler function operating on the PipelineBuilder in the
    // passes map. This registering is analogous to the normalizers (`code -> code`) but only operated using
    // side effects that change the pipeline.
    world.DLOG("compilation using {} : {}", compilation, compilation->type());

    // We can not directly access compile axms here.
    // But the compile plugin has not the necessary communication pipeline.
    // Therefore, we register the handlers and let the compile plugin call them.

    PipelineBuilder pipe_builder(world);
    auto pipeline     = compilation->as<Lam>()->body();
    auto [ax, phases] = collect_args(pipeline);

    // handle pipeline like all other pass axms
    auto pipeline_axm   = ax->as<Axm>();
    auto pipeline_flags = pipeline_axm->flags();
    world.DLOG("Building pipeline");
    if (auto pass = world.driver().pass(pipeline_flags))
        (*pass)(world, pipe_builder, pipeline);
    else
        error("pipeline_axm not found in passes");

    world.DLOG("Executing pipeline");
    pipe_builder.run_pipeline();

    return;
}

} // namespace mim
