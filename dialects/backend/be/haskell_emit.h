#pragma once

#include <ostream>

#include "thorin/phase/phase.h"

namespace thorin {

class World;

namespace backend {

namespace haskell {
void emit(World&, std::ostream&);
}

class HaskellEmitter : public Phase {
public:
    HaskellEmitter(World& world)
        : Phase(world, "haskell_emitter", false)
        , os_(std::move(std::cout)) {}

    void start() override { haskell::emit(world(), os_); }

private:
    std::ostream&& os_;
};

} // namespace backend
} // namespace thorin
