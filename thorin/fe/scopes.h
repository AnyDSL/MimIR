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
    void bind(Scope*, Sym sym, const Def*);
    void bind(Sym sym, const Def* def) { bind(&scopes_.back(), sym, def); }
    void merge(Scopes&);
    void swap(Scope& other) { std::swap(scopes_.back(), other); }

private:
    std::deque<Scope> scopes_;
};

} // namespace thorin::fe
