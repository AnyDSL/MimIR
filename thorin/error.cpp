#include "thorin/error.h"

#include "thorin/lam.h"

#include "thorin/util/print.h"

namespace thorin {

void ErrorHandler::expected_shape(const Def* def, const Def* dbg) {
    Debug d(dbg ? dbg : def->dbg());
    err(d.loc, "expected shape but got '{}' of type '{}'", def, def->type());
}

void ErrorHandler::expected_type(const Def* def, const Def* dbg) {
    Debug d(dbg ? dbg : def->dbg());
    err(d.loc, "expected type but got '{}' which is a term", def);
}

void ErrorHandler::index_out_of_range(const Def* arity, const Def* index, const Def* dbg) {
    Debug d(dbg ? dbg : index->dbg());
    err(d.loc, "index '{}' does not fit within arity '{}'", index, arity);
}

void ErrorHandler::index_out_of_range(const Def* arity, nat_t index, const Def* dbg) {
    Debug d(dbg);
    err(d.loc, "index '{}' does not fit within arity '{}'", index, arity);
}

void ErrorHandler::ill_typed_app(const Def* callee, const Def* arg, const Def* dbg) {
    Debug d(dbg ? dbg : arg->dbg());
    err(d.loc, "cannot pass argument '{}' of type '{}' to '{}' of domain '{}'", arg, arg->type(), callee,
        callee->type()->as<Pi>()->dom());
}

} // namespace thorin
