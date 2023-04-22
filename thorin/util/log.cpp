#include "thorin/util/log.h"

namespace thorin {

// clang-format off
std::string_view Log::level2acro(Level level) {
    switch (level) {
        case Level::Debug:   return "D";
        case Level::Verbose: return "V";
        case Level::Info:    return "I";
        case Level::Warn:    return "W";
        case Level::Error:   return "E";
        default: unreachable();
    }
}

Log::Level Log::str2level(std::string_view s) {
    if (false) {}
    else if (s == "debug"  ) return Level::Debug;
    else if (s == "verbose") return Level::Verbose;
    else if (s == "info"   ) return Level::Info;
    else if (s == "warn"   ) return Level::Warn;
    else if (s == "error"  ) return Level::Error;
    else error("invalid log level");
}

int Log::level2color(Level level) {
    switch (level) {
        case Level::Debug:   return 4;
        case Level::Verbose: return 4;
        case Level::Info:    return 2;
        case Level::Warn:    return 3;
        case Level::Error:   return 1;
        default: unreachable();
    }
}
// clang-format on

#if THORIN_COLOR_TERM
std::string Log::colorize(std::string_view str, int color) {
    if (isatty(fileno(stdout))) {
        const char c = '0' + color;
        return "\033[1;3" + (c + ('m' + std::string(str))) + "\033[0m";
    }
    return std::string(str);
}
#else
std::string Log::colorize(std::string_view str, int) { return std::string(str); }
#endif

} // namespace thorin
