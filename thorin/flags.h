#pragma once

namespace thorin {

// Compiler switches that must be saved and looked up in later phases of compilation.
struct Flags {
    bool dump_gid = false;
#if THORIN_ENABLE_CHECKS
    bool reeval_breakpoints = false;
    bool trace              = false;
#endif
};

} // namespace thorin
