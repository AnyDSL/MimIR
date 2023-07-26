#include "dialects/mem/pass/fp/ssa_destr.h"

namespace thorin::mem {

void SSADestr::enter() { world().ILOG("hi!"); }

Ref SSADestr::rewrite(const Proxy* proxy) { return proxy; }

Ref SSADestr::rewrite(Ref def) { return def; }

undo_t SSADestr::analyze(const Proxy*) { return No_Undo; }

undo_t SSADestr::analyze(Ref) { return No_Undo; }

} // namespace thorin::mem
