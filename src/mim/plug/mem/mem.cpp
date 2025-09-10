#include "mim/plug/mem/mem.h"

#include <mim/config.h>
#include <mim/pass.h>

#include <mim/pass/beta_red.h>
#include <mim/pass/eta_exp.h>

#include "mim/plug/mem/mem.h"
#include "mim/plug/mem/pass/copy_prop.h"
#include "mim/plug/mem/pass/reshape.h"
#include "mim/plug/mem/pass/ssa.h"
#include "mim/plug/mem/phase/add_mem.h"
#include "mim/plug/mem/phase/alloc2malloc.h"
#include "mim/plug/mem/phase/remem_elim.h"

using namespace mim;
using namespace mim::plug;

void reg_stages(Flags2Stages& stages) {
    // clang-format off
    Stage::hook<mem::remem_repl,        mem::phase::RememElim   >(stages);
    Stage::hook<mem::alloc2malloc_repl, mem::phase::Alloc2Malloc>(stages);
    Stage::hook<mem::add_mem_phase,     mem::phase::AddMem      >(stages);
    Stage::hook<mem::ssa_pass,          mem::pass:: SSA         >(stages);
    Stage::hook<mem::copy_prop_pass,    mem::pass:: CopyProp    >(stages);
    Stage::hook<mem::reshape_pass,      mem::pass:: Reshape     >(stages);
    // clang-format on
}

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"mem", mem::register_normalizers, reg_stages, nullptr}; }
