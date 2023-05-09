#include "thorin/fe/scopes.h"

#include "thorin/world.h"

namespace thorin::fe {

void Scopes::pop() {
    assert(!scopes_.empty());
    scopes_.pop_back();
}

const Def* Scopes::query(Dbg dbg) const {
    if (dbg.sym == '_') return nullptr;

    for (auto& scope : scopes_ | std::ranges::views::reverse)
        if (auto i = scope.find(dbg.sym); i != scope.end()) return i->second.second;

    return nullptr;
}

const Def* Scopes::find(Dbg dbg) const {
    if (dbg.sym == '_') error(dbg.loc, "the symbol '_' is special and never binds to anything");
    if (auto res = query(dbg)) return res;
    error(dbg.loc, "'{}' not found", dbg.sym);
}

void Scopes::bind(Scope* scope, Dbg dbg, const Def* def, bool rebind) {
    auto [loc, sym] = dbg;
    if (sym == '_') return; // don't do anything with '_'

    if (rebind) {
        (*scope)[sym] = std::pair(loc, def);
    } else if (auto [i, ins] = scope->emplace(sym, std::pair(loc, def)); !ins) {
        auto prev = i->second.first;
        error(loc, "redeclaration of '{}'; previous declaration here: {}", sym, prev);
    }
}

} // namespace thorin::fe
