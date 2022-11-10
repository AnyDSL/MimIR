#include "dialects/mem/passes/rw/add_mem.h"

#include <cassert>

#include <iostream>

#include <thorin/lam.h>

#include "thorin/axiom.h"
#include "thorin/def.h"

#include "dialects/core/core.h"
#include "dialects/mem/mem.h"

namespace thorin::mem {

const Def* AddMem::rewrite_structural(const Def* def) {
    auto& world = def->world();

    return Rewriter::rewrite_structural(def); // continue recursive rewriting with everything else
}

} // namespace thorin::mem
