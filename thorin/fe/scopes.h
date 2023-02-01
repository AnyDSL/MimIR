#pragma once

#include <deque>

#include "thorin/debug.h"

namespace thorin::fe {

class Scopes {
public:
    using Scope = SymMap<const Def*>;

    Scopes() { push(); /* root scope */ }

    void push() { scopes_.emplace_back(); }
    void pop();
    Scope* curr() { return &scopes_.back(); }
    const Def* find(Sym) const;
    void bind(Scope*, Sym sym, const Def*, bool rebind = false);
    void bind(Sym sym, const Def* def, bool rebind = false) { bind(&scopes_.back(), sym, def, rebind); }
    void merge(Scopes&);
    void swap(Scope& other) { std::swap(scopes_.back(), other); }

private:
    std::deque<Scope> scopes_;
};

} // namespace thorin::fe
