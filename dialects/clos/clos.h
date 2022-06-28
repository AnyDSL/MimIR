#pragma once

#include "thorin/world.h"

#include "dialects/clos/autogen.h"

namespace thorin::clos {

inline const Def* op_alloc_jumpbuf(const Def* mem, const Def* dbg = {}) {
    World& w = mem->world();
    return w.app(w.ax<alloc_jmpbuf>(), {w.tuple(), mem}, dbg);
}
inline const Def* op_setjmp(const Def* mem, const Def* buf, const Def* dbg = {}) {
    World& w = mem->world();
    return w.app(w.ax<setjmp>(), {mem, buf}, dbg);
}
inline const Def* op_longjmp(const Def* mem, const Def* buf, const Def* id, const Def* dbg = {}) {
    World& w = mem->world();
    return w.app(w.ax<longjmp>(), {mem, buf, id}, dbg);
}
inline const Def* op(clos o, const Def* def, const Def* dbg = {}) {
    World& w = def->world();
    return w.app(w.app(w.ax(o), def->type()), def, dbg);
}

} // namespace thorin::clos
