#include "thorin/fe/scopes.h"

#include "thorin/world.h"

namespace thorin::fe {

void Scopes::pop() {
    assert(!scopes_.empty());
    scopes_.pop_back();
}

const Def* Scopes::find(Dbg dbg) const {
    auto [loc, sym] = dbg;
    if (sym == '_') err(loc, "the symbol '_' is special and never binds to anything", sym);

    for (auto& scope : scopes_ | std::ranges::views::reverse) {
        if (auto i = scope.find(sym); i != scope.end()) return i->second.second;
    }

    err(loc, "symbol '{}' not found", sym);
}

void Scopes::bind(Scope* scope, Dbg dbg, const Def* def, bool rebind) {
    auto [loc, sym] = dbg;
    if (sym == '_') return; // don't do anything with '_'

    if (rebind) {
        (*scope)[sym] = std::pair(loc, def);
    } else if (auto [i, ins] = scope->emplace(sym, std::pair(loc, def)); !ins) {
        auto prev = i->second.first;
        err(loc, "symbol '{}' already declared in the current scope here: {}", sym, prev);
    }
}

void Scopes::merge(Scopes& other) {
    assert(scopes_.size() == 1 && other.scopes_.size() == 1);
    scopes_.front().merge(other.scopes_.front());
}

} // namespace thorin::fe
