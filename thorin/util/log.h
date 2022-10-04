#pragma once

#include "thorin/debug.h"

#include "thorin/util/print.h"

namespace thorin {

struct Log {
    enum class Level { Error, Warn, Info, Verbose, Debug };

    template<class... Args>
    void log(Level level, Loc loc, const char* fmt, Args&&... args) const {
        if (ostream && int(level) <= int(this->level)) {
            std::ostringstream oss;
            oss << loc;
            print(*ostream, "{}:{}: ", colorize(level2acro(level), level2color(level)), colorize(oss.str(), 7));
            print(*ostream, fmt, std::forward<Args&&>(args)...) << std::endl;
        }
    }
    void log() const {} ///< Dummy for Debug build.

    static std::string_view level2acro(Level);
    static Level str2level(std::string_view);
    static int level2color(Level level);
    static std::string colorize(std::string_view str, int color);

    std::ostream* ostream = nullptr;
    Level level           = Level::Error; ///< **Maximum** log Level.
};

// clang-format off
#define ELOG(...) log().log(thorin::Log::Level::Error,   thorin::Loc(__FILE__, {__LINE__, thorin::u32(-1)}, {__LINE__, thorin::u32(-1)}), __VA_ARGS__)
#define WLOG(...) log().log(thorin::Log::Level::Warn,    thorin::Loc(__FILE__, {__LINE__, thorin::u32(-1)}, {__LINE__, thorin::u32(-1)}), __VA_ARGS__)
#define ILOG(...) log().log(thorin::Log::Level::Info,    thorin::Loc(__FILE__, {__LINE__, thorin::u32(-1)}, {__LINE__, thorin::u32(-1)}), __VA_ARGS__)
#define VLOG(...) log().log(thorin::Log::Level::Verbose, thorin::Loc(__FILE__, {__LINE__, thorin::u32(-1)}, {__LINE__, thorin::u32(-1)}), __VA_ARGS__)
#ifndef NDEBUG
#define DLOG(...) log().log(thorin::Log::Level::Debug,   thorin::Loc(__FILE__, {__LINE__, thorin::u32(-1)}, {__LINE__, thorin::u32(-1)}), __VA_ARGS__)
#else
#define DLOG(...) log().log()
#endif
// clang-format on

} // namespace thorin
