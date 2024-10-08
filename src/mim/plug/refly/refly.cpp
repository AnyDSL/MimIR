#include "mim/plug/refly/refly.h"

#include <mim/config.h>
#include <mim/pass/pass.h>
#include <mim/pass/pipelinebuilder.h>

#include "mim/plug/refly/pass/remove_perm.h"

using namespace mim;

extern "C" THORIN_EXPORT Plugin mim_get_plugin() {
    return {
        "refly", [](Normalizers& normalizers) { plug::refly::register_normalizers(normalizers); },
        [](Passes& passes) { register_pass<plug::refly::remove_dbg_perm_pass, plug::refly::RemoveDbgPerm>(passes); },
        nullptr};
}
