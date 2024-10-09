#include "mim/plug/compile/pass/debug_print.h"

#include <mim/lam.h>

#include "mim/plug/compile/compile.h"

namespace mim::plug::compile {

void DebugPrint::enter() {
    if (level >= 2) world().DLOG("L{}: enter {}", level, curr_mut());
}

} // namespace mim::plug::compile
