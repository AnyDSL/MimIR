#include "dialects/mem/pass/fp/ssa_destr.h"

#include "dialects/mem/mem.h"

namespace thorin::mem {

void SSADestr::enter() {
    for (auto var : curr_mut()->vars())
        if (match<mem::M>(var->type())) mem_ = var;
    // world().ILOG("hi!: {}, {}", curr_mut(),
}

Ref SSADestr::rewrite(const Proxy* proxy) { return proxy; }

Ref SSADestr::rewrite(Ref def) { return def; }

undo_t SSADestr::analyze(const Proxy*) { return No_Undo; }

undo_t SSADestr::analyze(Ref) { return No_Undo; }

} // namespace thorin::mem
