#ifndef THORIN_CHECK_H
#define THORIN_CHECK_H

#include <deque>

#include "thorin/def.h"

namespace thorin {

class Checker {
public:
    Checker(World& world)
        : world_(world) {}

    World& world() const { return world_; }
    bool equiv(const Def*, const Def*);
    bool assignable(const Def*, const Def*);

private:
    World& world_;
    DefDefSet equiv_;
    std::deque<DefDef> vars_;
};

} // namespace thorin

#endif
