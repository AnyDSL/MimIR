#include "mim/plug/phase/pass/debug_print.h"

#include <mim/lam.h>

#include "mim/plug/phase/phase.h"

namespace mim::plug::phase {

void DebugPrint::enter() {
    if (level_ >= 2) world().DLOG("L{}: enter {}", level_, curr_mut());
}

} // namespace mim::plug::phase
