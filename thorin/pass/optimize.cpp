#include "thorin/pass/optimize.h"

#include <vector>

#include "thorin/dialects.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/pipelinebuilder.h"
#include "thorin/pass/rw/lam_spec.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"
#include "thorin/phase/phase.h"

namespace thorin {

/// See optimize.h for magic numbers
void optimize(World& world, Passes& passes, std::vector<Dialect>& dialects) {
    auto compilation_functions = {"_compile", "_default_compile", "_core_compile", "_fallback_compile"};
    const Def* compilation     = nullptr;
    for (auto compilation_function : compilation_functions) {
        if (auto compilation_ = world.lookup(compilation_function)) {
            if (!compilation) { compilation = compilation_; }
            compilation_->make_internal();
        }
    }
    // make all functions `[] -> Pipeline` internal
    std::vector<Def*> make_internal;
    for (auto ext : world.externals()) {
        auto def = ext.second;
        if (auto lam = def->isa<Lam>(); lam && lam->num_doms() == 0) {
            if (*lam->codom()->name() == "Pipeline") {
                if (!compilation) { compilation = lam; }
                make_internal.push_back(def);
            }
        }
    }
    for (auto def : make_internal) { def->make_internal(); }
    assert(compilation && "no compilation function found");

    // We found a compilation directive in the file and use it to build the compilation pipeline.
    // The general idea is that passes and phases are exposed as axioms.
    // Each pass/phase axiom is associated with a handler function operating on the PipelineBuilder in the
    // passes map. This registering is analogous to the normalizers (`code -> code`) but only operated using
    // side effects that change the pipeline.
    world.DLOG("compilation using {} : {}", compilation, compilation->type());

    // We can not directly access compile axioms here.
    // But the compile dialect has not the necessary communication pipeline.
    // Therefore, we register the handlers and let the compile dialect call them.

    PipelineBuilder pipe_builder(world);
    // TODO: remove indirections of pipeline builder. Just add passes and phases directly to the pipeline.
    for (auto& dialect : dialects) { pipe_builder.register_dialect(dialect); }

    auto pipeline     = compilation->as<Lam>()->body();
    auto [ax, phases] = collect_args(pipeline);

    // handle pipeline like all other pass axioms
    auto pipeline_axiom = ax->as<Axiom>();
    auto pipeline_flags = pipeline_axiom->flags();
    assert(passes.contains(pipeline_flags));
    world.DLOG("Building pipeline");
    passes[pipeline_flags](world, pipe_builder, pipeline);

    world.DLOG("Executing pipeline");
    pipe_builder.run_pipeline();

    return;
}

} // namespace thorin
