#pragma once

#include <ostream>

#include "thorin/phase/phase.h"

namespace thorin {

class World;

namespace backend {

namespace rust {
void emit(World&, std::ostream&);
}

class RustEmitter : public Phase {
public:
    RustEmitter(World& world)
        : Phase(world, "rust_emitter", false)
        , os_(std::move(std::cout)) {}

    void start() override { rust::emit(world(), os_); }

private:
    std::ostream&& os_;
};

} // namespace backend
} // namespace thorin
