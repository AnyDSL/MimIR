#pragma once

#include <ostream>

#include "thorin/phase/phase.h"

namespace thorin {

class World;

namespace backend {

namespace ocaml {
void emit(World&, std::ostream&);
}

class OCamlEmitter : public Phase {
public:
    OCamlEmitter(World& world)
        : Phase(world, "ocaml_emitter", false)
        , os_(std::move(std::cout)) {}

    void start() override { ocaml::emit(world(), os_); }

private:
    std::ostream&& os_;
};

} // namespace backend
} // namespace thorin
