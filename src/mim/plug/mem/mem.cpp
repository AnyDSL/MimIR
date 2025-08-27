#include "mim/plug/mem/mem.h"

#include <mim/config.h>

#include <mim/pass/beta_red.h>
#include <mim/pass/eta_exp.h>
#include <mim/pass/pass.h>

#include "mim/plug/mem/autogen.h"
#include "mim/plug/mem/pass/alloc2malloc.h"
#include "mim/plug/mem/pass/copy_prop.h"
#include "mim/plug/mem/pass/remem_elim.h"
#include "mim/plug/mem/pass/reshape.h"
#include "mim/plug/mem/pass/ssa_constr.h"
#include "mim/plug/mem/phase/add_mem.h"

using namespace mim;
using namespace mim::plug;

void reg_stages(Phases& phases, Passes& passes) {
    Pipeline::hook<mem::add_mem_phase, mem::AddMem>(phases);

    PassMan::hook<mem::ssa_pass, mem::SSAConstr>(passes);
    PassMan::hook<mem::remem_elim_pass, mem::RememElim>(passes);
    PassMan::hook<mem::alloc2malloc_pass, mem::Alloc2Malloc>(passes);

    passes[flags_t(Annex::Base<mem::copy_prop_pass>)] = [&](PassMan& man, const Def* app) {
        auto bb_only = Lit::as(app->as<App>()->arg());
        app->world().DLOG("registering copy_prop with bb_only = {}", bb_only);
        man.add<mem::CopyProp>(bb_only);
    };

    passes[flags_t(Annex::Base<mem::reshape_pass>)] = [&](PassMan& man, const Def* app) {
        auto mode_axm = app->as<App>()->arg()->as<Axm>();
        auto mode
            = mode_axm->flags() == flags_t(Annex::Base<mem::reshape_arg>) ? mem::Reshape::Arg : mem::Reshape::Flat;
        man.add<mem::Reshape>(mode);
    };
}

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"mem", [](Normalizers& n) { mem::register_normalizers(n); }, reg_stages, nullptr};
}
