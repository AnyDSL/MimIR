#include "dialects/refly/refly.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>
#include <thorin/pass/pipelinebuilder.h>

#include "dialects/refly/passes/remove_perm.h"

using namespace thorin;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"refly", [](Normalizers& normalizers) { refly::register_normalizers(normalizers); },
        [](Passes& passes) { register_pass<refly::remove_dbg_perm_pass, refly::RemoveDbgPerm>(passes); }, nullptr};
}
