#ifndef DIALECTS_MEM_MEM_H
#define DIALECTS_MEM_MEM_H

#include "thorin/lam.h"

#include "dialects/mem.h"

namespace thorin::mem {
const Def* mem_var(Lam* lam, const Def* dbg = nullptr);
} // namespace thorin::mem

#endif