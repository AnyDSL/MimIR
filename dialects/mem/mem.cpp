#include "dialects/mem/mem.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_exp.h"
#include "thorin/pass/fp/eta_red.h"
#include "thorin/pass/fp/tail_rec_elim.h"
#include "thorin/pass/rw/partial_eval.h"
#include "thorin/pass/rw/ret_wrap.h"
#include "thorin/pass/rw/scalarize.h"

#include "dialects/mem/autogen.h"
#include "dialects/mem/passes/fp/copy_prop.h"
#include "dialects/mem/passes/fp/ssa_constr.h"
#include "dialects/mem/passes/rw/alloc2malloc.h"
#include "dialects/mem/passes/rw/remem_elim.h"
#include "dialects/mem/passes/rw/reshape.h"
#include "dialects/mem/phases/rw/add_mem.h"

using namespace thorin;

extern "C" THORIN_EXPORT DialectInfo thorin_get_dialect_info() {
    return {"mem",
            [](Passes& passes) {
                register_pass_with_arg<mem::ssa_pass, mem::SSAConstr, EtaExp>(passes);
                register_pass<mem::remem_elim_pass, mem::RememElim>(passes);
                register_pass<mem::alloc2malloc_pass, mem::Alloc2Malloc>(passes);

                // TODO: generalize register_pass_with_arg
                passes[flags_t(Axiom::Base<mem::copy_prop_pass>)] = [&](World& world, PipelineBuilder& builder,
                                                                        const Def* app) {
                    auto [br, ee, bb] = app->as<App>()->args<3>();
                    // TODO: let get_pass do the casts
                    auto br_pass = (BetaRed*)builder.get_pass_instance(br);
                    auto ee_pass = (EtaExp*)builder.get_pass_instance(ee);
                    auto bb_only = bb->as<Lit>()->get<u64>();
                    world.DLOG("registering copy_prop with br = {}, ee = {}, bb_only = {}", br, ee, bb_only);
                    builder.add_pass<mem::CopyProp>(app, br_pass, ee_pass, bb_only);
                };
            },
            nullptr, [](Normalizers& normalizers) { mem::register_normalizers(normalizers); }};
}
