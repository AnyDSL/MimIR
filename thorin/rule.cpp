#include "thorin/rule.h"

#include "thorin/world.h"

namespace thorin {

RuleType::RuleType(World& world)
    : Def(Node, world.type(), Defs{}, 0, nullptr) {}

} // namespace thorin
