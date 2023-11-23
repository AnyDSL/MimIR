#include "thorin/plug/mem/mem.h"

#include <thorin/config.h>
#include <thorin/pass/fp/beta_red.h>
#include <thorin/pass/fp/eta_exp.h>
#include <thorin/pass/pass.h>
#include <thorin/pass/pipelinebuilder.h>

#include "thorin/plug/mem/autogen.h"
#include "thorin/plug/mem/pass/fp/copy_prop.h"
#include "thorin/plug/mem/pass/fp/ssa_constr.h"
#include "thorin/plug/mem/pass/rw/alloc2malloc.h"
#include "thorin/plug/mem/pass/rw/remem_elim.h"
#include "thorin/plug/mem/pass/rw/reshape.h"
#include "thorin/plug/mem/phase/add_mem.h"

using namespace thorin;
using namespace thorin::plug;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"mem", [](Normalizers& normalizers) { mem::register_normalizers(normalizers); },
            [](Passes& passes) {
                register_pass_with_arg<mem::ssa_pass, mem::SSAConstr, EtaExp>(passes);
                register_pass<mem::remem_elim_pass, mem::RememElim>(passes);
                register_pass<mem::alloc2malloc_pass, mem::Alloc2Malloc>(passes);

                // TODO: generalize register_pass_with_arg
                passes[flags_t(Annex::Base<mem::copy_prop_pass>)]
                    = [&](World& world, PipelineBuilder& builder, const Def* app) {
                          auto [br, ee, bb] = app->as<App>()->args<3>();
                          // TODO: let get_pass do the casts
                          auto br_pass = (BetaRed*)builder.pass(br);
                          auto ee_pass = (EtaExp*)builder.pass(ee);
                          auto bb_only = bb->as<Lit>()->get<u64>();
                          world.DLOG("registering copy_prop with br = {}, ee = {}, bb_only = {}", br, ee, bb_only);
                          builder.add_pass<mem::CopyProp>(app, br_pass, ee_pass, bb_only);
                      };
                passes[flags_t(Annex::Base<mem::reshape_pass>)]
                    = [&](World&, PipelineBuilder& builder, const Def* app) {
                          auto mode_ax = app->as<App>()->arg()->as<Axiom>();
                          auto mode    = mode_ax->flags() == flags_t(Annex::Base<mem::reshape_arg>) ? mem::Reshape::Arg
                                                                                                    : mem::Reshape::Flat;
                          builder.add_pass<mem::Reshape>(app, mode);
                      };
                register_phase<mem::add_mem_phase, mem::AddMem>(passes);
            },
            nullptr};
}
