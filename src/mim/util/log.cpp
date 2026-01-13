#include "mim/util/log.h"

namespace mim {

// clang-format off
char Log::level2acro(Level level) {
    switch (level) {
        case Level::Trace:   return 'T';
        case Level::Debug:   return 'D';
        case Level::Verbose: return 'V';
        case Level::Info:    return 'I';
        case Level::Warn:    return 'W';
        case Level::Error:   return 'E';
        default: fe::unreachable();
    }
}

rang::fg Log::level2color(Level level) {
    switch (level) {
        case Level::Trace:   return rang::fg::magenta;
        case Level::Debug:   return rang::fg::cyan;
        case Level::Verbose: return rang::fg::blue;
        case Level::Info:    return rang::fg::green;
        case Level::Warn:    return rang::fg::yellow;
        case Level::Error:   return rang::fg::red;
        default: fe::unreachable();
    }
}
// clang-format on

} // namespace mim
