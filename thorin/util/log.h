#pragma once

#include <ostream>

#include "thorin/flags.h"

#include "thorin/util/loc.h"

namespace thorin {

/// Facility to log what you are doing.
/// @see @ref fmt "Formatted Output", @ref log "Logging Macros"
class Log {
public:
    Log(const Flags& flags)
        : flags_(flags) {}

    enum class Level { Error, Warn, Info, Verbose, Debug };

    /// @name Getters
    ///@{
    const Flags& flags() const { return flags_; }
    Level level() const { return max_level_; }
    std::ostream& ostream() const {
        assert(ostream_);
        return *ostream_;
    }
    explicit operator bool() const { return ostream_; } ///< Checks if Log::ostream_ is set.
    ///@}

    /// @name Setters
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

    /// @name Log
    ///@{
    /// Output @p fmt to Log::ostream; does nothing if Log::ostream is `nullptr`.
    /// @see @ref fmt "Formatted Output", @ref log "Logging Macros"
    template<class... Args>
    void log(Level level, Loc loc, const char* fmt, Args&&... args) const {
        if (ostream_ && level <= max_level_) {
            std::ostringstream oss;
            oss << loc;
            print(ostream(), "{}:{}: ", colorize(level2acro(level), level2color(level)), colorize(oss.str(), 7));
            print(ostream(), fmt, std::forward<Args&&>(args)...) << std::endl;
            if ((level == Level::Error && flags().break_on_error) || (level == Level::Warn && flags().break_on_warn))
                breakpoint();
        }
    }
    template<class... Args>
    void log(Level level, const char* file, uint16_t line, const char* fmt, Args&&... args) {
        auto path = fs::path(file);
        log(level, Loc(&path, line), fmt, std::forward<Args&&>(args)...);
    }
    ///@}

    /// @name Conversions
    ///@{
    static std::string_view level2acro(Level);
    static Level str2level(std::string_view);
    static int level2color(Level level);
    static std::string colorize(std::string_view str, int color);
    ///@}

private:
    const Flags& flags_;
    std::ostream* ostream_ = nullptr;
    Level max_level_       = Level::Error;
};

/// @name Logging Macros
/// @anchor log
///@{
/// Macros for different thorin::Log::Level%s for ease of use.
/// @see @ref fmt "Formatted Output"
// clang-format off
#define ELOG(...) log().log(thorin::Log::Level::Error,   __FILE__, __LINE__, __VA_ARGS__)
#define WLOG(...) log().log(thorin::Log::Level::Warn,    __FILE__, __LINE__, __VA_ARGS__)
#define ILOG(...) log().log(thorin::Log::Level::Info,    __FILE__, __LINE__, __VA_ARGS__)
#define VLOG(...) log().log(thorin::Log::Level::Verbose, __FILE__, __LINE__, __VA_ARGS__)
/// Vaporizes to nothingness in `Debug` build.
#ifndef NDEBUG
#define DLOG(...) log().log(thorin::Log::Level::Debug,   __FILE__, __LINE__, __VA_ARGS__)
#else
#define DLOG(...) dummy()
#endif
// clang-format on
///@}

} // namespace thorin
