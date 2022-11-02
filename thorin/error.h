#pragma once

#include <sstream>
#include <stdexcept>

#include "thorin/debug.h"

#include "thorin/util/print.h"
#include "thorin/util/types.h"

namespace thorin {

class Def;

template<class T = std::logic_error, class... Args>
[[noreturn]] void err(const char* fmt, Args&&... args) {
    std::ostringstream oss;
    print(oss << "error: ", fmt, std::forward<Args&&>(args)...);
    throw T(oss.str());
}

template<class T = std::logic_error, class... Args>
[[noreturn]] void err(Loc loc, const char* fmt, Args&&... args) {
    std::ostringstream oss;
    print(oss, "{}: error: ", loc);
    print(oss, fmt, std::forward<Args&&>(args)...);
    throw T(oss.str());
}

class ErrorHandler {
public:
    virtual ~ErrorHandler() = default;

    virtual void expected_shape(const Def* def, const Def* dbg);
    virtual void expected_type(const Def* def, const Def* dbg);
    virtual void index_out_of_range(const Def* arity, const Def* index, const Def* dbg);
    virtual void index_out_of_range(const Def* arity, nat_t index, const Def* dbg);
    virtual void ill_typed_app(const Def* callee, const Def* arg, const Def* dbg);

    /// Place holder until we have better methods.
    template<class T = std::logic_error, class... Args>
    [[noreturn]] void err(Loc loc, const char* fmt, Args&&... args) {
        thorin::err(loc, fmt, std::forward<Args&&>(args)...);
    }
};

} // namespace thorin
