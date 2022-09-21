#pragma once

namespace thorin {

// Compiler switches that must be saved and looked up in later phases of compilation.
struct Flags {
    int dump_gid        = 0;
    bool dump_recursive = false;
#if THORIN_ENABLE_CHECKS
    bool reeval_breakpoints = false;
    bool trace_gids         = false;
#endif
};

} // namespace thorin
