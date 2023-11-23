#include "thorin/plug/compile/pass/debug_print.h"

#include <thorin/lam.h>

#include "thorin/plug/compile/compile.h"

namespace thorin::plug::compile {

void DebugPrint::enter() {
    if (level >= 2) world().DLOG("L{}: enter {}", level, curr_mut());
}

} // namespace thorin::plug::compile
