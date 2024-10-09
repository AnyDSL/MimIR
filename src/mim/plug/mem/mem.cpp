#include "mim/plug/mem/mem.h"

#include <mim/config.h>

#include <mim/pass/beta_red.h>
#include <mim/pass/eta_exp.h>
#include <mim/pass/pass.h>
#include <mim/pass/pipelinebuilder.h>

#include "mim/plug/mem/autogen.h"
#include "mim/plug/mem/pass/alloc2malloc.h"
#include "mim/plug/mem/pass/copy_prop.h"
#include "mim/plug/mem/pass/remem_elim.h"
#include "mim/plug/mem/pass/reshape.h"
#include "mim/plug/mem/pass/ssa_constr.h"
#include "mim/plug/mem/phase/add_mem.h"

using namespace mim;
using namespace mim::plug;

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
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
