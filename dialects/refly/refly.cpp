#include "dialects/refly/refly.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>
#include <thorin/pass/pipelinebuilder.h>

#include "dialects/refly/passes/remove_perm.h"

using namespace thorin;

/// heart of the dialect
/// registers passes in the different optimization phases
/// as well as normalizers for the axioms
extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"refly", [](Passes& passes) { register_pass<refly::remove_dbg_perm_pass, refly::RemoveDbgPerm>(passes); },
            nullptr, [](Normalizers& normalizers) { refly::register_normalizers(normalizers); }};
}

// TODO: check (and fix) for windows
#define YELLOW "\033[0;33m"
#define BLANK  "\033[0m"

namespace thorin::refly {
void debug_print(const Def* def) {
    auto& world = def->world();
    world.DLOG(YELLOW "debug_print: {}" BLANK, def);
    world.DLOG("def : {}", def);
    world.DLOG("id  : {}", def->unique_name());
    world.DLOG("type: {}", def->type());
    world.DLOG("node: {}", def->node_name());
    world.DLOG("ops : {}", def->num_ops());
    world.DLOG("proj: {}", def->num_projs());
    world.DLOG("eops: {}", def->num_extended_ops());
}
} // namespace thorin::refly
