#pragma once

#include <ostream>

#include "thorin/phase/phase.h"

namespace thorin {

class World;

namespace haskell {

void emit(World&, std::ostream&);

class OCamlEmitter : public Phase {
public:
    OCamlEmitter(World& world)
        : Phase(world, "ocaml_emitter", false) {}

    void start() override { emit(world(), std::cout); }
};

} // namespace haskell
} // namespace thorin
