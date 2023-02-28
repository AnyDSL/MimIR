#include "dialects/autodiff/passes/autodiff_ext_cleanup.h"

#include <iostream>

#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/core/core.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

void AutoDiffExternalCleanup::enter() {
    Lam* lam = curr_nom();
    if (lam->name()->starts_with("internal_diff_")) {
        lam->make_internal();
        world().DLOG("internalized {}", lam);
    }
}

} // namespace thorin::autodiff
