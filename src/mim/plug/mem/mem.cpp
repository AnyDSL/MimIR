#include "mim/plug/mem/mem.h"

#include <mim/config.h>
#include <mim/pass.h>

#include <mim/pass/beta_red.h>
#include <mim/pass/eta_exp.h>

#include "mim/plug/mem/autogen.h"
#include "mim/plug/mem/pass/alloc2malloc.h"
#include "mim/plug/mem/pass/copy_prop.h"
#include "mim/plug/mem/pass/remem_elim.h"
#include "mim/plug/mem/pass/reshape.h"
#include "mim/plug/mem/pass/ssa_constr.h"
#include "mim/plug/mem/phase/add_mem.h"

using namespace mim;
using namespace mim::plug;

void reg_stages(Flags2Phases& phases, Flags2Passes& passes) {
    PhaseMan::hook<mem::add_mem_phase, mem::AddMem>(phases);
    // clang-format off
    PassMan::hook<mem::ssa_pass,          mem::SSAConstr   >(passes);
    PassMan::hook<mem::remem_elim_pass,   mem::RememElim   >(passes);
    PassMan::hook<mem::alloc2malloc_pass, mem::Alloc2Malloc>(passes);
    PassMan::hook<mem::copy_prop_pass,    mem::CopyProp    >(passes);
    PassMan::hook<mem::reshape_pass,      mem::Reshape     >(passes);
    // clang-format on
}

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"mem", mem::register_normalizers, reg_stages, nullptr}; }
