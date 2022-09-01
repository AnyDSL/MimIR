#include "dialects/debug/passes/remove_perm.h"

#include <iostream>

#include <thorin/lam.h>
#include <thorin/tables.h>

#include "dialects/debug/debug.h"

namespace thorin::debug {

const Def* DebugRemovePerm::rewrite(const Def* def) {
    if (auto dbg_perm_app = match<dbg_perm>(def)) {
        auto e = dbg_perm_app->arg();
        world().DLOG("dbg_perm: {}", e);
        return e;
    }

    return def;
}

} // namespace thorin::debug
