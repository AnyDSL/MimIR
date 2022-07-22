#pragma once

#include "thorin/debug.h"

#include "thorin/util/print.h"

namespace thorin {

struct Log {
    enum class Level { Error, Warn, Info, Verbose, Debug };

    template<class... Args>
    void log(Level curr, Loc loc, const char* fmt, Args&&... args) {
        if (ostream && int(curr) <= int(level)) {
            std::ostringstream oss;
            oss << loc;
            print(*ostream, "{}:{}: ", colorize(level2acro(curr), level2color(curr)), colorize(oss.str(), 7));
            print(*ostream, fmt, std::forward<Args&&>(args)...) << std::endl;
        }
    }

    template<class... Args>
    [[noreturn]] void error(Loc loc, const char* fmt, Args&&... args) {
        log(Level::Error, loc, fmt, std::forward<Args&&>(args)...);
        std::abort();
    }

    static std::string_view level2acro(Level);
    static Level str2level(std::string_view);
    static int level2color(Level level);
    static std::string colorize(std::string_view str, int color);

    std::ostream* ostream = nullptr;
    Level level           = Level::Error; ///< **Maximum** log Level.
};

} // namespace thorin
