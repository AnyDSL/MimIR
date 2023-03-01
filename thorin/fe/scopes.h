#pragma once

#include <deque>

#include "thorin/util/loc.h"

namespace thorin {

class Def;

namespace fe {

class Scopes {
public:
    using Scope = SymMap<std::pair<Loc, const Def*>>;

    Scopes() { push(); /* root scope */ }

    void push() { scopes_.emplace_back(); }
    void pop();
    Scope* curr() { return &scopes_.back(); }
    const Def* find(Dbg) const;
    void bind(Scope*, Dbg, const Def*, bool rebind = false);
    void bind(Dbg dbg, const Def* def, bool rebind = false) { bind(&scopes_.back(), dbg, def, rebind); }
    void merge(Scopes&);
    void swap(Scope& other) { std::swap(scopes_.back(), other); }

private:
    std::deque<Scope> scopes_;
};

}
}
