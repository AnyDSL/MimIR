#include "mim/plug/refly/refly.h"

#include <mim/config.h>

#include <mim/pass/pass.h>

#include "mim/plug/refly/pass/remove_perm.h"

using namespace mim;

void reg_stages(Phases&, Passes& passes) {
    PassMan::hook<plug::refly::remove_dbg_perm_pass, plug::refly::RemoveDbgPerm>(passes);
}

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"refly", [](Normalizers& n) { plug::refly::register_normalizers(n); }, reg_stages, nullptr};
}
