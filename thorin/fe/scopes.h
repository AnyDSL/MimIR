#pragma once

#include <deque>

#include "thorin/util/dbg.h"

namespace thorin {

class Def;
class Ptrn;

class Scopes {
public:
    using Scope = fe::SymMap<std::pair<Loc, const Def*>>;

    Scopes() { push(); /* root scope */ }

    void push() { scopes_.emplace_back(); }
    void pop();
    Scope* curr() { return &scopes_.back(); }
    const Def* query(Dbg) const;
    const Def* find(Dbg) const; ///< Same as Scopes::query but potentially raises an error.
    void bind(Scope*, Dbg, const Def*, bool rebind = false);
    void bind(Dbg dbg, const Def* def, bool rebind = false) { bind(&scopes_.back(), dbg, def, rebind); }
    void swap(Scope& other) { std::swap(scopes_.back(), other); }

private:
    std::deque<Scope> scopes_;
};

} // namespace thorin
