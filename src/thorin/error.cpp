#include "thorin/error.h"

#include <stdexcept>

#include "thorin/lam.h"

#include "thorin/util/stream.h"

namespace thorin {

template<class... Args>
[[noreturn]] void err(const char* fmt, Args&&... args) {
    StringStream s;
    s.fmt(fmt, std::forward<Args&&>(args)...);
    throw std::logic_error(s.str());
}

void ErrorHandler::expected_shape(const Def* def) {
    err("exptected shape but got '{}' of type '{}'", def, def->type());
}

void ErrorHandler::index_out_of_range(const Def* arity, const Def* index) {
    err("index '{}' does not fit within arity '{}'", index, arity);
}

void ErrorHandler::ill_typed_app(const Def* callee, const Def* arg) {
    err("cannot pass argument '{} of type '{}' to '{}' of domain '{}'", arg, arg->type(), callee,
        callee->type()->as<Pi>()->dom());
}

} // namespace thorin
