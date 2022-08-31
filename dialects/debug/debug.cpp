#include "dialects/debug/debug.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"
#include "dialects/debug/passes/remove_perm.h"

using namespace thorin;

/// heart of the dialect
/// registers passes in the different optimization phases
/// as well as normalizers for the axioms
extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"debug",
            [](thorin::PipelineBuilder& builder) {
                builder.extend_codegen_prep_phase([](PassMan& man) {
                    man.add<thorin::debug::DebugRemovePerm>(); 
                });
            },
            nullptr, [](Normalizers& normalizers) { debug::register_normalizers(normalizers); }};
}

namespace thorin::debug {
    void debug_print(const Def* def) {
        auto& world = def->world();
        world.DLOG("\033[0;33mdebug_print: {}\033[0m", def);
        world.DLOG("def : {}", def);
        world.DLOG("id  : {}", def->unique_name());
        world.DLOG("type: {}", def->type());
        world.DLOG("node: {}", def->node_name());
        world.DLOG("ops : {}", def->num_ops());
        world.DLOG("proj: {}", def->num_projs());
        world.DLOG("eops: {}", def->num_extended_ops());
    }
}