#include "dialects/tool/passes/set_filter.h"

#include <iostream>

#include <thorin/lam.h>
#include <thorin/tables.h>

#include "dialects/tool/tool.h"

namespace thorin::tool {

const Def* SetFilter::rewrite(const Def* def) {
    if (auto set_filter_app = match<set_filter>(def)) {
        auto [filter, v] = set_filter_app->args<2>();
        world().DLOG("set_filter: {} for expr {}", filter, v);
        Lam* lam = curr_nom()->as<Lam>();
        world().DLOG("lambda: {}", lam);
        world().DLOG("current filter is: {}", lam->filter());
        lam->set_filter(filter);
        // assert(0);
        return v;
    }

    return def;
}

} // namespace thorin::tool
