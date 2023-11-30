#include "thorin/plug/refly/pass/remove_perm.h"

#include "thorin/plug/refly/refly.h"

namespace thorin::plug::refly {

Ref RemoveDbgPerm::rewrite(Ref def) {
    if (auto dbg_perm = match(dbg::perm, def)) {
        auto [lvl, x] = dbg_perm->args<2>();
        world().DLOG("dbg_perm: {}", x);
        return x;
    }

    return def;
}

} // namespace thorin::plug::refly
