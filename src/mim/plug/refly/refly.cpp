#include "mim/plug/refly/refly.h"

#include <mim/config.h>
#include <mim/pass.h>

#include "mim/plug/refly/pass/remove_perm.h"

using namespace mim;

void reg_stages(Flags2Phases&, Flags2Passes& passes) {
    PassMan::hook<plug::refly::remove_dbg_perm_pass, plug::refly::RemoveDbgPerm>(passes);
}

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"refly", plug::refly::register_normalizers, reg_stages, nullptr};
}
