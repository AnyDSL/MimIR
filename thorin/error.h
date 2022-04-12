#ifndef THORIN_ERROR_H
#define THORIN_ERROR_H

#include <sstream>
#include <stdexcept>

#include "thorin/debug.h"

#include "thorin/util/print.h"

namespace thorin {

class Def;

class LexError : public std::logic_error {
public:
    LexError(const std::string& what_arg)
        : std::logic_error(what_arg) {}
};

class ParseError : public std::logic_error {
public:
    ParseError(const std::string& what_arg)
        : std::logic_error(what_arg) {}
};

class ScopeError : public std::logic_error {
public:
    ScopeError(const std::string& what_arg)
        : std::logic_error(what_arg) {}
};

class TypeError : public std::logic_error {
public:
    TypeError(const std::string& what_arg)
        : std::logic_error(what_arg) {}
};

template<class T, class... Args>
[[noreturn]] void err(Loc loc, const char* fmt, Args&&... args) {
    std::ostringstream oss;
    print(oss, "{}: error: ", loc);
    print(oss, fmt, std::forward<Args&&>(args)...);
    throw T(oss.str());
}

class ErrorHandler {
public:
    virtual ~ErrorHandler() = default;

    virtual void expected_shape(const Def* def);
    virtual void index_out_of_range(const Def* arity, const Def* index);
    virtual void ill_typed_app(const Def* callee, const Def* arg);
};

} // namespace thorin

#endif
