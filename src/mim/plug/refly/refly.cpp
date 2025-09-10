#include "mim/plug/refly/refly.h"

#include <mim/config.h>
#include <mim/pass.h>

#include "mim/plug/refly/phase/remove_dbg.h"

using namespace mim;

void reg_stages(Flags2Stages& stages) {
    PassMan::hook<plug::refly::remove_dbg_repl, plug::refly::phase::RemoveDbg>(stages);
}

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"refly", plug::refly::register_normalizers, reg_stages, nullptr};
}
