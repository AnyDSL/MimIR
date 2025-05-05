#include "mim/plug/refly/pass/remove_perm.h"

#include "mim/plug/refly/refly.h"

namespace mim::plug::refly {

const Def* RemoveDbgPerm::rewrite(const Def* def) {
    if (auto dbg_perm = test(dbg::perm, def)) {
        auto [lvl, x] = dbg_perm->args<2>();
        world().DLOG("dbg_perm: {}", x);
        return x;
    }

    return def;
}

} // namespace mim::plug::refly
