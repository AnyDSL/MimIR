#pragma once

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
    Error()             = default;
    Error(const Error&) = default;
    Error(Error&& other)
        : Error() {
        swap(*this, other);
    }
    /// Creates a single Tag::Error message.
    Error(Loc loc, const std::string& str)
        : msgs_{
            {loc, Tag::Error, str}
    } {}
    ///@}

    /// @name Getters
    ///@{
    const auto& msgs() const { return msgs_; }
    size_t num_msgs() const {
        assert(num_errors() + num_warnings() + num_notes() == msgs_.size());
        return msgs_.size();
    }
    size_t num_errors() const { return num_errors_; }
    size_t num_warnings() const { return num_warnings_; }
    size_t num_notes() const { return num_notes_; }
    ///@}

    void clear() {
        num_errors_   = 0;
        num_warnings_ = 0;
        num_notes_    = 0;
        msgs_.clear();
    }

    /// If errors occured, claim them and throw.
    void ack() {
        if (num_errors() != 0) {
            auto errors = std::move(*this);
            throw errors;
        }
    }

    /// @name Add formatted message
    ///@{
    template<class... Args> Error& msg(Loc loc, Tag tag, const char* s, Args&&... args) {
        msgs_.emplace_back(loc, tag, fmt(s, std::forward<Args&&>(args)...));
        return *this;
    }

    // clang-format off
    template<class... Args> Error& error(Loc loc, const char* s, Args&&... args) { ++num_errors_;   return msg(loc, Tag::Error, s, std::forward<Args&&>(args)...); }
    template<class... Args> Error& warn (Loc loc, const char* s, Args&&... args) { ++num_warnings_; return msg(loc, Tag::Warn,  s, std::forward<Args&&>(args)...); }
    template<class... Args> Error& note (Loc loc, const char* s, Args&&... args) {
        assert(num_errors() > 0 || num_warnings() > 0); /*                      */ ++num_notes_;    return msg(loc, Tag::Note,  s, std::forward<Args&&>(args)...);
    }
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

    friend void swap(Error& e1, Error& e2) noexcept {
        using std::swap;
        ;
        // clang-format off
        swap(e1.msgs_,         e2.msgs_);
        swap(e1.num_errors_,   e2.num_errors_);
        swap(e1.num_warnings_, e2.num_warnings_);
        swap(e1.num_notes_,    e2.num_notes_);
        // clang-format on
    }

private:
    std::vector<Msg> msgs_;
    size_t num_errors_   = 0;
    size_t num_warnings_ = 0;
    size_t num_notes_    = 0;
};

/// @name Formatted Output
///@{
/// Single Error that `throw`s immediately.
template<class... Args> [[noreturn]] void error(Loc loc, const char* f, Args&&... args) {
    throw Error(loc, fmt(f, std::forward<Args&&>(args)...));
}
///@}

struct Dbg {
public:
    /// @name Constructors
    ///@{
    constexpr Dbg() noexcept           = default;
    constexpr Dbg(const Dbg&) noexcept = default;
    constexpr Dbg(Loc loc, Sym sym) noexcept
        : loc_(loc)
        , sym_(sym) {}
    constexpr Dbg(Loc loc) noexcept
        : Dbg(loc, {}) {}
    constexpr Dbg(Sym sym) noexcept
        : Dbg({}, sym) {}
    Dbg& operator=(const Dbg&) noexcept = default;
    ///@}

    /// @name Getters
    ///@{
    Sym sym() const { return sym_; }
    Loc loc() const { return loc_; }
    bool is_anon() const { return !sym() || sym() == '_'; }
    explicit operator bool() const { return sym().operator bool(); }
    ///@}

    /// @name Setters
    ///@{
    Dbg& set(Sym sym) { return sym_ = sym, *this; }
    Dbg& set(Loc loc) { return loc_ = loc, *this; }
    ///@}

private:
    Loc loc_;
    Sym sym_;

    friend std::ostream& operator<<(std::ostream& os, const Dbg& dbg) { return os << dbg.sym(); }
};

} // namespace thorin
