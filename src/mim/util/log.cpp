#include "mim/util/log.h"

namespace mim {

// clang-format off
std::string_view Log::level2acro(Level level) {
    switch (level) {
        case Level::Debug:   return "D";
        case Level::Verbose: return "V";
        case Level::Info:    return "I";
        case Level::Warn:    return "W";
        case Level::Error:   return "E";
        default: fe::unreachable();
    }
}

rang::fg Log::level2color(Level level) {
    switch (level) {
        case Level::Debug:   return rang::fg::yellow;
        case Level::Verbose: return rang::fg::cyan;
        case Level::Info:    return rang::fg::green;
        case Level::Warn:    return rang::fg::magenta;
        case Level::Error:   return rang::fg::red;
        default: fe::unreachable();
    }
}
// clang-format on

} // namespace mim
