#pragma once

#include "thorin/config.h"

namespace thorin {

// Compiler switches that must be saved and looked up in later phases of compilation.
struct Flags {
    int dump_gid               = 0;
    bool dump_recursive        = false;
    bool disable_type_checking = false; // TODO implement this flag
    bool bootstrap             = false;
    bool aggressive_lam_spec   = false; // HACK makes LamSpec more agressive but potentially non-terminating
#if THORIN_ENABLE_CHECKS
    bool reeval_breakpoints = false;
    bool trace_gids         = false;
#endif
};

} // namespace thorin
