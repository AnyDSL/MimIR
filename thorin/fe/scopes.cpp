#include "thorin/fe/scopes.h"

#include "thorin/world.h"

namespace thorin::fe {

void Scopes::pop() {
    assert(!scopes_.empty());
    scopes_.pop_back();
}

const Def* Scopes::find(Sym sym) const {
    if (sym.is_anonymous()) err(sym.loc(), "the symbol '_' is special and never binds to anything", sym);

    for (auto& scope : scopes_ | std::ranges::views::reverse) {
        if (auto i = scope.find(sym); i != scope.end()) return i->second;
    }

    err(sym.loc(), "symbol '{}' not found", sym);
}

void Scopes::bind(Scope* scope, Sym sym, const Def* def) {
    if (sym.is_anonymous()) return; // don't do anything with '_'

    if (auto [i, ins] = scope->emplace(sym, def); !ins) {
        auto curr = sym.loc();
        auto prev = i->first.to_loc();
        err(curr, "symbol '{}' already declared in the current scope here: {}", sym, prev);
    }
}

void Scopes::merge(Scopes& other) {
    assert(scopes_.size() == 1 && other.scopes_.size() == 1);
    scopes_.front().merge(other.scopes_.front());
}

} // namespace thorin::fe
