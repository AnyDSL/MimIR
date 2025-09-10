#include "mim/plug/refly/phase/remove_dbg.h"

#include "mim/plug/refly/refly.h"

namespace mim::plug::refly::phase {

const Def* RemoveDbg::rewrite(const Def* def) {
    if (auto dbg_perm = Axm::isa(dbg::perm, def)) {
        auto [lvl, x] = dbg_perm->args<2>();
        DLOG("dbg_perm: {}", x);
        return rewrite(x);
    }

    return Rewriter::rewrite(def);
}

} // namespace mim::plug::refly::phase
