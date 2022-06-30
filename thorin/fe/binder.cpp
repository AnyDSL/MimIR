#include "thorin/fe/binder.h"

#include "thorin/world.h"

namespace thorin {

Binder::Binder(World& world)
    : anonymous_(world.tuple_str("_"), nullptr) {
    push(); // root scope
}

void Binder::pop() {
    assert(!scopes_.empty());
    scopes_.pop_back();
}

const Def* Binder::find(Sym sym) const {
    if (sym == anonymous_) err<ScopeError>(sym.loc(), "the symbol '_' is special and never binds to anything", sym);

    for (auto& scope : scopes_ | std::ranges::views::reverse) {
        if (auto i = scope.find(sym); i != scope.end()) return i->second;
    }

    err<ScopeError>(sym.loc(), "symbol '{}' not found", sym);
}

void Binder::bind(Sym sym, const Def* def) {
    if (sym == anonymous_) return; // don't do anything with '_'

    if (auto [i, ins] = scopes_.back().emplace(sym, def); !ins) {
        auto curr = sym.loc();
        auto prev = i->first.to_loc();
        err<ScopeError>(curr, "symbol '{}' already declared in the current scope here: {}", sym, prev);
    }
}

void Binder::merge(Binder& other) {
    assert(scopes_.size() == 1 && other.scopes_.size() == 1);
    scopes_.front().merge(other.scopes_.front());
}

} // namespace thorin
