#pragma once

#include <deque>

#include "thorin/debug.h"

namespace thorin::fe {

class Scopes {
public:
    Scopes(World&);

    void push() { scopes_.emplace_back(); }
    void pop();
    const Def* find(Sym) const;
    void bind(Sym sym, const Def*);
    void merge(Scopes&);
    Sym anonymous() const { return anonymous_; }

private:
    using Scope = SymMap<const Def*>;

    std::deque<Scope> scopes_;
    Sym anonymous_;
};

using Binders = std::deque<std::pair<Sym, size_t>>;

} // namespace thorin::fe
