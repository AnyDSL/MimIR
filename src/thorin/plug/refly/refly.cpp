#include "thorin/plug/refly/refly.h"

#include <thorin/config.h>

#include <thorin/pass/pass.h>
#include <thorin/pass/pipelinebuilder.h>

#include "thorin/plug/refly/pass/remove_perm.h"

using namespace thorin;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {
        "refly", [](Normalizers& normalizers) { plug::refly::register_normalizers(normalizers); },
        [](Passes& passes) { register_pass<plug::refly::remove_dbg_perm_pass, plug::refly::RemoveDbgPerm>(passes); },
        nullptr};
}
