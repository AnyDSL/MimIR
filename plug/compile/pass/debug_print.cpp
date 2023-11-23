#include "plug/compile/pass/debug_print.h"

#include <thorin/lam.h>

#include "plug/compile/compile.h"

namespace thorin::compile {

void DebugPrint::enter() {
    if (level >= 2) world().DLOG("L{}: enter {}", level, curr_mut());
}

} // namespace thorin::compile
