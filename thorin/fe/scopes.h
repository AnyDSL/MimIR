#pragma once

#include <deque>

#include "thorin/debug.h"

namespace thorin::fe {

class Scopes {
public:
    Scopes() { push(); /* root scope */ }

    void push() { scopes_.emplace_back(); }
    void pop();
    const Def* find(Sym) const;
    void bind(Sym sym, const Def*);
    void merge(Scopes&);

private:
    using Scope = SymMap<const Def*>;

    std::deque<Scope> scopes_;
};

using Binders = std::deque<std::pair<Sym, size_t>>;

} // namespace thorin::fe
