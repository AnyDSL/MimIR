#pragma once

#include "thorin/def.h"
#include "thorin/lam.h"
#include "thorin/world.h"

#include "dialects/autodiff/builder.h"
#include "dialects/mem/mem.h"

namespace thorin {
class World;

class Builder {
    World& world_;
    DefVec ops_;
    bool filter_ = false;

public:
    Builder(World& world)
        : world_(world) {}

    Builder& insert(size_t i, const Def* def) {
        if (def != nullptr) { ops_.insert(ops_.begin() + i, def); }
        return *this;
    }

    Builder& add(const Def* def) {
        if (def != nullptr) { ops_.push_back(def); }
        return *this;
    }

    Builder& add(DefArray ops) {
        ops_.insert(std::end(ops_), std::begin(ops), std::end(ops));
        return *this;
    }

    const Def* tuple() { return world_.tuple(ops_); }

    const Pi* cn() { return world_.cn(ops_); }

    const Def* sigma() { return world_.sigma(ops_); }

    Builder& mem_ty() { return add(mem::type_mem(world_)); }

    Builder& arr_ty(const Def* shape, const Def* body) { return add(world_.arr(shape, body)); }

    Builder& mem_cn() { return add(world_.cn(mem::type_mem(world_))); }

    Builder& with_filter() {
        filter_ = true;
        return *this;
    }

    Lam* lam(const std::string& name) {
        Lam* lam = world_.nom_lam(cn(), world_.dbg(name));
        lam->set_filter(world_.lit_bool(filter_));
        return lam;
    }
};

static Builder build(World& world) { return Builder(world); }
} // namespace thorin