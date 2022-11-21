#include "dialects/compile/passes/debug_print.h"

#include <thorin/lam.h>

#include "dialects/compile/compile.h"
#include "dialects/mem/mem.h"

namespace thorin::compile {

void DebugPrint::enter() {
    // if (level >= 2) {
    world().DLOG("L{}: enter {}", level, curr_nom());
    // }
}

// const Def* DebugPrint::rewrite(const Def* def) {}

} // namespace thorin::compile
