#include "dialects/direct/passes/ds2cps.h"

#include <thorin/lam.h>

#include "dialects/direct/direct.h"

namespace thorin::direct {

void DS2CPS::enter() {
    Lam* prev = currentLambda;
    currentLambda = curr_nom();

    currentLambda->set_body(rewrite_(currentLambda->body()));
    
    currentLambda = prev;
}

const Def* DS2CPS::rewrite_(const Def* def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;

    return def;
}

PassTag* DS2CPS::ID() {
    static PassTag Key;
    return &Key;
}

} // namespace thorin::direct
