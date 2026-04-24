#include <mim/plug/eqsat/phase/egg_rewrite.h>
#include <mim/plug/eqsat/phase/eqsat_phase.h>
#include <mim/plug/eqsat/phase/slotted_rewrite.h>

namespace mim::plug::eqsat {

void EqsatPhase::start() {
    bool SLOTTED = false;

    if (SLOTTED) {
        SlottedRewrite slotted_rewrite(world(), "slotted_rewrite");
        slotted_rewrite.start();
    } else {
        EggRewrite egg_rewrite(world(), "egg_rewrite");
        egg_rewrite.start();
    }
}

}; // namespace mim::plug::eqsat
