#pragma once

#include "mim/world.h"

namespace mim {

class World;
class Driver;
class Log;

/// Common base for Phase and Pass.
class Stage : public fe::RuntimeCast<Stage> {
public:
    /// @name Construction & Destruction
    ///@{
    Stage(World& world, std::string name)
        : world_(world)
        , name_(std::move(name)) {}
    Stage(World& world, flags_t annex);
    virtual ~Stage() {}

    virtual Stage* recreate() = 0; ///< Creates a new instance; needed by a fixed-point PhaseMan.
    ///@}

    /// @name Getters
    ///@{
    World& world() { return world_; }
    Driver& driver() { return world().driver(); }
    Log& log() const { return world_.log(); }
    std::string_view name() const { return name_; }
    flags_t annex() const { return annex_; }
    ///@}

private:
    World& world_;
    flags_t annex_ = 0;

protected:
    std::string name_;
};

} // namespace mim
