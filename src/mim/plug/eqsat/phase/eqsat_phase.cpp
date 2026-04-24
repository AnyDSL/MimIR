#include <mim/plug/eqsat/phase/egg_rewrite.h>
#include <mim/plug/eqsat/phase/eqsat_phase.h>
#include <mim/plug/eqsat/phase/slotted_rewrite.h>

namespace mim::plug::eqsat {

void EqsatPhase::start() {
    // TODO: Do we get this condition from a compiler flag or
    //       should we extract it from a config function?
    // - if we add a compiler flag --slotted, then we should
    //   also use this to output slotted-sexprs by combining
    //   '--output-sexpr --slotted' and get rid of '--output-sexpr-slotted'
    bool SLOTTED = true;

    if (SLOTTED) {
        SlottedRewrite slotted_rewrite(world(), "slotted_rewrite");
        slotted_rewrite.start();
    } else {
        EggRewrite egg_rewrite(world(), "egg_rewrite");
        egg_rewrite.start();
    }
}

}; // namespace mim::plug::eqsat
