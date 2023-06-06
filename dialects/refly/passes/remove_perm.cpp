#include "dialects/refly/passes/remove_perm.h"

#include "dialects/refly/refly.h"

namespace thorin::refly {

Ref RemoveDbgPerm::rewrite(Ref def) {
    if (auto dbg_perm = match(dbg::perm, def)) {
        auto [lvl, x] = dbg_perm->args<2>();
        world().DLOG("dbg_perm: {}", x);
        return x;
    }

    return def;
}

} // namespace thorin::refly
