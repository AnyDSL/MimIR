#pragma once

#include <ostream>

#include "thorin/util/loc.h"

namespace thorin {

class Log {
public:
    enum class Level { Error, Warn, Info, Verbose, Debug };

    /// @name getters
    ///@{
    Level level() const { return max_level_; }
    std::ostream& ostream() const {
        assert(ostream_);
        return *ostream_;
    }
    explicit operator bool() const { return ostream_; } ///< Checks if Log::ostream_ is set.
    ///@}

    /// @name setters
    ///@{
    Log& set(std::ostream* ostream) {
        ostream_ = ostream;
        return *this;
    }
    Log& set(Level max_level) {
        max_level_ = max_level;
        return *this;
    }
    ///@}

    /// @name log
    ///@{
    /// Output @p fmt to Log::ostream; does nothing if Log::ostream is `nullptr`.
    template<class... Args>
    void log(Level level, Loc loc, const char* fmt, Args&&... args) const {
        if (ostream_ && level <= max_level_) {
            std::ostringstream oss;
            oss << loc;
            print(ostream(), "{}:{}: ", colorize(level2acro(level), level2color(level)), colorize(oss.str(), 7));
            print(ostream(), fmt, std::forward<Args&&>(args)...) << std::endl;
        }
    }
    template<class... Args>
    void log(Level level, const char* file, uint16_t line, const char* fmt, Args&&... args) {
        auto path = fs::path(file);
        log(level, Loc(&path, line), fmt, std::forward<Args&&>(args)...);
    }
    ///@}

    /// @name conversions
    ///@{
    static std::string_view level2acro(Level);
    static Level str2level(std::string_view);
    static int level2color(Level level);
    static std::string colorize(std::string_view str, int color);
    ///@}

private:
    std::ostream* ostream_ = nullptr;
    Level max_level_       = Level::Error;
};

/// @name Macros for differen Log Levels for ease of use
///@{
// clang-format off
#define ELOG(...) log().log(thorin::Log::Level::Error,   __FILE__, __LINE__, __VA_ARGS__)
#define WLOG(...) log().log(thorin::Log::Level::Warn,    __FILE__, __LINE__, __VA_ARGS__)
#define ILOG(...) log().log(thorin::Log::Level::Info,    __FILE__, __LINE__, __VA_ARGS__)
#define VLOG(...) log().log(thorin::Log::Level::Verbose, __FILE__, __LINE__, __VA_ARGS__)
#ifndef NDEBUG
#define DLOG(...) log().log(thorin::Log::Level::Debug,   __FILE__, __LINE__, __VA_ARGS__)
#else
#define DLOG(...) dummy()
#endif
// clang-format on
///@}

} // namespace thorin
