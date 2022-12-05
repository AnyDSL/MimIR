#include "dialects/refly/passes/remove_perm.h"

#include "dialects/refly/refly.h"

namespace thorin::refly {

const Def* RemoveDbgPerm::rewrite(const Def* def) {
    if (auto dbg_perm = match(dbg::perm, def)) {
        auto e = dbg_perm->arg();
        world().DLOG("dbg_perm: {}", e);
        return e;
    }

    return def;
}

} // namespace thorin::refly
