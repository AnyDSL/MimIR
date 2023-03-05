#pragma once

#include "thorin/util/loc.h"

namespace thorin {

class Log {
public:
    enum class Level { Error, Warn, Info, Verbose, Debug };

    Log(SymPool& sym_pool)
        : sym_pool_(sym_pool) {}

    /// @name log
    ///@{
    /// Output @p fmt to Log::ostream; does nothing if Log::ostream is `nullptr`.
    template<class... Args>
    void log(Level level, Loc loc, const char* fmt, Args&&... args) const {
        if (ostream && int(level) <= int(this->level)) {
            std::ostringstream oss;
            oss << loc;
            print(*ostream, "{}:{}: ", colorize(level2acro(level), level2color(level)), colorize(oss.str(), 7));
            print(*ostream, fmt, std::forward<Args&&>(args)...) << std::endl;
        }
    }
    template<class... Args>
    void log(Level level, const char* file, uint16_t line, const char* fmt, Args&&... args) {
        log(level, Loc(sym_pool_.sym(file), line), fmt, std::forward<Args&&>(args)...);
    }
    void log() const {} ///< Dummy for Debug build.
    ///@}

    /// @name conversions
    ///@{
    static std::string_view level2acro(Level);
    static Level str2level(std::string_view);
    static int level2color(Level level);
    static std::string colorize(std::string_view str, int color);
    ///@}

    /// @name Log Level and output stream as public members
    ///@{
    std::ostream* ostream = nullptr;
    Level level           = Level::Error; ///< **Maximum** log Level.
    ///@}

private:
    SymPool& sym_pool_;
};

// clang-format off
#define ELOG(...) log().log(thorin::Log::Level::Error,   __FILE__, __LINE__, __VA_ARGS__)
#define WLOG(...) log().log(thorin::Log::Level::Warn,    __FILE__, __LINE__, __VA_ARGS__)
#define ILOG(...) log().log(thorin::Log::Level::Info,    __FILE__, __LINE__, __VA_ARGS__)
#define VLOG(...) log().log(thorin::Log::Level::Verbose, __FILE__, __LINE__, __VA_ARGS__)
#ifndef NDEBUG
#define DLOG(...) log().log(thorin::Log::Level::Debug,   __FILE__, __LINE__, __VA_ARGS__)
#else
#define DLOG(...)
#endif
// clang-format on

} // namespace thorin
