#pragma once

#include "thorin/world.h"

namespace thorin {

class Phase {
public:
    Phase(World& world, const char* name)
        : world_(world)
        , name_(name) {}

    World& world() { return world_; }
    const char* name() const { return name_; }

    virtual void run();
    virtual void start() = 0;

protected:
    World& world_;
    const char* name_;
};

class RewritePhase : public Phase {
protected:
    RewritePhase(World& world, const char* name)
        : Phase(world, name)
        , new_(world.state())
    {}

    void start() override;
    virtual const Def* rewrite(const Def*);
    virtual std::pair<const Def*, bool> pre_rewrite(const Def*) { return {nullptr, false}; }
    virtual std::pair<const Def*, bool> post_rewrite(const Def*) { return {nullptr, false}; }

protected:
    World new_;
    Def2Def old2new_;
};

class Cleanup : public RewritePhase {
public:
    Cleanup(World& world)
        : RewritePhase(world, "cleanup") {}
};

}
