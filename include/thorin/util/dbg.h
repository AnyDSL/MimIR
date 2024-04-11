#pragma once

#include <filesystem>
#include <stdexcept>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <fe/loc.h>
#include <fe/sym.h>
#include <rang.hpp>

#include "thorin/util/print.h"

namespace thorin {

using fe::Loc;
using fe::Pos;
using fe::Sym;

class Error : std::exception {
public:
    enum class Tag {
        Error,
        Warn,
        Note,
    };

    struct Msg {
        Loc loc;
        Tag tag;
        std::string str;

        friend std::ostream& operator<<(std::ostream& os, const Msg& msg) {
            return print(os, "{}{}: {}: {}{}", rang::fg::yellow, msg.loc, msg.tag, rang::fg::reset, msg.str);
            return print(os, "{}{}: {}: {}{}", rang::fg::yellow, msg.loc, msg.tag, rang::fg::reset, msg.str);
        }
    };

    /// @name Constructors
    ///@{
    Error() = default;
    Error(Loc loc, const std::string& str) ///< Creates a single Tag::Error message.
        : msgs_{
            {loc, Tag::Error, str}
    } {}
    ///@}

    /// @name Getters
    ///@{
    const auto& msgs() const { return msgs_; }
    int num_errors() const { return num_errors_; }
    int num_warnings() const { return num_warnings_; }
    int num_notes() const { return num_notes_; }
    ///@}

    /// @name Add formatted message
    ///@{
    template<class... Args> Error& msg(Loc loc, Tag tag, const char* s, Args&&... args) {
        msgs_.emplace_back(loc, tag, fmt(s, std::forward<Args&&>(args)...));
        return *this;
    }

    // clang-format off
    template<class... Args> Error& error(Loc loc, const char* s, Args&&... args) { ++num_errors_;   return msg(loc, Tag::Error, s, std::forward<Args&&>(args)...); }
    template<class... Args> Error& warn (Loc loc, const char* s, Args&&... args) { ++num_warnings_; return msg(loc, Tag::Warn,  s, std::forward<Args&&>(args)...); }
    template<class... Args> Error& note (Loc loc, const char* s, Args&&... args) { ++num_notes_;    return msg(loc, Tag::Note,  s, std::forward<Args&&>(args)...); }
    // clang-format on
    //@}

    friend std::ostream& operator<<(std::ostream& o, Tag tag) {
        // clang-format off
        switch (tag) {
            case Tag::Error: return o << rang::fg::red     << "error";
            case Tag::Warn:  return o << rang::fg::magenta << "warning";
            case Tag::Note:  return o << rang::fg::green   << "note";
            default: fe::unreachable();
        }
        // clang-format on
    }

    friend std::ostream& operator<<(std::ostream& os, const Error& e) {
        for (const auto& msg : e.msgs()) os << msg << std::endl;
        return os;
    }

private:
    std::vector<Msg> msgs_;
    int num_errors_   = 0;
    int num_warnings_ = 0;
    int num_notes_    = 0;
};

/// @name Formatted Output
///@{
/// Single Error that `throw`s immediately.
template<class... Args> [[noreturn]] void error(Loc loc, const char* f, Args&&... args) {
    throw Error(loc, fmt(f, std::forward<Args&&>(args)...));
}
///@}

struct Dbg {
    constexpr Dbg() noexcept           = default;
    constexpr Dbg(const Dbg&) noexcept = default;
    constexpr Dbg(Loc loc, Sym sym) noexcept
        : loc(loc)
        , sym(sym) {}
    constexpr Dbg(Loc loc) noexcept
        : Dbg(loc, {}) {}
    Dbg& operator=(const Dbg&) noexcept = default;

    Loc loc;
    Sym sym;

    explicit operator bool() const { return sym.operator bool(); }

    friend std::ostream& operator<<(std::ostream& os, const Dbg& dbg) { return os << dbg.sym; }
};

} // namespace thorin
