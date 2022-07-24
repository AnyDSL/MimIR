#pragma once

#include <functional>
#include <iostream>
#include <ostream>
#include <ranges>
#include <sstream>
#include <string>

#include "thorin/util/assert.h"

namespace thorin {

template<class R>
std::ostream& range(std::ostream& os, const R& r, const char* sep = ", ") {
    return range(
        os, r, [&os](const auto& x) { os << x; }, sep);
}

template<class R, class F>
std::ostream& range(std::ostream& os, const R& r, F f, const char* sep = ", ") {
    const char* cur_sep = "";
    for (const auto& elem : r) {
        for (auto i = cur_sep; *i != '\0'; ++i) os << *i;
        f(elem);
        cur_sep = sep;
    }
    return os;
}

template<class R, class F>
struct Elem {
    static constexpr bool is_elem = true;

    Elem(const R& range, const F& f)
        : range(range)
        , f(f) {}

    const R range;
    const F f;
};

template<typename T>
concept Elemable = requires(T elem)
{
    elem.range;
    elem.f;
};

bool match2nd(std::ostream& os, const char* next, const char*& s, const char c);
std::ostream& print(std::ostream& os, const char* s);

template<class T, class... Args>
std::ostream& print(std::ostream& os, const char* s, T&& t, Args&&... args) {
    while (*s != '\0') {
        auto next = s + 1;

        switch (*s) {
            case '{': {
                if (match2nd(os, next, s, '{')) continue;
                s++; // skip opening brace '{'

                std::string spec;
                while (*s != '\0' && *s != '}') spec.push_back(*s++);
                assert(*s == '}' && "unmatched closing brace '}' in format string");

                if constexpr (std::is_invocable_v<decltype(t)>) {
                    std::invoke(t);
                } else if constexpr (std::ranges::range<decltype(t)>) {
                    range(os, t, spec.c_str());
                } else if constexpr (Elemable<decltype(t)>) {
                    range(os, t.range, t.f, spec.c_str());
                } else {
                    os << t;
                }

                ++s; // skip closing brace '}'
                // call even when *s == '\0' to detect extra arguments
                return print(os, s, std::forward<Args&&>(args)...);
            }
            case '}':
                if (match2nd(os, next, s, '}')) continue;
                assert(false && "unmatched/unescaped closing brace '}' in format string");
                unreachable();
            default: os << *s++;
        }
    }

    assert(false && "invalid format string for 's'");
    unreachable();
}

template<class... Args>
std::string fmt(const char* s, Args&&... args) {
    std::ostringstream os;
    print(os, s, std::forward<Args&&>(args)...);
    return os.str();
}

// clang-format off
template<class... Args> std::ostream& outf (const char* fmt, Args&&... args) { return print(std::cout, fmt, std::forward<Args&&>(args)...); }
template<class... Args> std::ostream& errf (const char* fmt, Args&&... args) { return print(std::cerr, fmt, std::forward<Args&&>(args)...); }
template<class... Args> std::ostream& outln(const char* fmt, Args&&... args) { return outf(fmt, std::forward<Args&&>(args)...) << std::endl; }
template<class... Args> std::ostream& errln(const char* fmt, Args&&... args) { return errf(fmt, std::forward<Args&&>(args)...) << std::endl; }
// clang-format on

class Tab {
public:
    Tab(std::string_view tab = {"    "}, size_t indent = 0)
        : tab_(tab)
        , indent_(indent) {}

    template<class... Args>
    std::ostream& print(std::ostream& os, const char* s, Args&&... args) {
        for (size_t i = 0; i < indent_; ++i) os << tab_;
        return thorin::print(os, s, std::forward<Args>(args)...);
    }
    /// Same as above but first puts a `std::endl` to @p os.
    template<class... Args>
    std::ostream& lnprint(std::ostream& os, const char* s, Args&&... args) {
        return print(os << std::endl, s, std::forward<Args>(args)...);
    }

    /// @name getters
    ///@{
    size_t indent() const { return indent_; }
    std::string_view tab() const { return tab_; }
    ///@}

    // clang-format off
    /// @name creates a new Tab
    ///@{
    [[nodiscard]] Tab operator++(int) const {                      return {tab_, indent_ + 1}; }
    [[nodiscard]] Tab operator--(int) const { assert(indent_ > 0); return {tab_, indent_ - 1}; }
    [[nodiscard]] Tab operator+(size_t indent) const {                      return {tab_, indent_ + indent}; }
    [[nodiscard]] Tab operator-(size_t indent) const { assert(indent_ > 0); return {tab_, indent_ - indent}; }
    ///@}

    /// @name modifies this Tab
    ///@{
    Tab& operator++() {                      ++indent_; return *this; }
    Tab& operator--() { assert(indent_ > 0); --indent_; return *this; }
    Tab& operator+=(size_t indent) {                      indent_ += indent; return *this; }
    Tab& operator-=(size_t indent) { assert(indent_ > 0); indent_ -= indent; return *this; }
    Tab& operator=(size_t indent) { indent_ = indent; return *this; }
    Tab& operator=(std::string_view tab) { tab_ = tab; return *this; }
    ///@}
    // clang-format on

private:
    std::string_view tab_;
    size_t indent_ = 0;
};

#ifdef NDEBUG
#    define assertf(condition, ...) \
        do { (void)sizeof(condition); } while (false)
#else
#    define assertf(condition, ...)                                     \
        do {                                                            \
            if (!(condition)) {                                         \
                thorin::errf("{}:{}: assertion: ", __FILE__, __LINE__); \
                thorin::errln(__VA_ARGS__);                             \
                thorin::breakpoint();                                   \
            }                                                           \
        } while (false)
#endif

} // namespace thorin
