#pragma once

#include <deque>

#include "thorin/def.h"

namespace thorin {

const Def* infer_type_level(World&, Defs);

class Checker {
public:
    Checker(World& world)
        : world_(world) {}

    World& world() const { return world_; }
    template<bool Cache = true>
    bool equiv(const Def*, const Def*, const Def*);
    bool assignable(const Def*, const Def*, const Def*);

private:
    World& world_;
    DefDefSet equiv_;
    std::deque<DefDef> vars_;
};

extern template bool Checker::equiv<true>(const Def*, const Def*, const Def*);
extern template bool Checker::equiv<false>(const Def*, const Def*, const Def*);

} // namespace thorin
