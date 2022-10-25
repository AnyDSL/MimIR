#pragma once

#include <queue>

#include "thorin/phase/phase.h"

namespace thorin::mem {

using DefQueue = std::deque<const Def*>;

class Reshape;

struct ReshapePlugin {
    std::function<const Def*(Reshape& reshape, const Def*)> rewrite;
};

class Reshape : public RWPass<Reshape, Lam> {
public:
    enum Mode { Flat, Arg };

    Reshape(PassMan& man, Mode mode)
        : RWPass(man, "reshape")
        , mode_(mode) {}

    void add_plugin(std::function<const Def*(Reshape& reshape, const Def*)> rewrite) {
        auto& plugin   = plugins_.emplace_back();
        plugin.rewrite = rewrite;
    }

    const Def* rewrite(const Def* def);
    const Def* convert_ty(const Def* ty);
    const Def* reshape(const Def* arg, const Pi* target_pi);

private:
    void enter() override;

    const Def* rewrite_convert(const Def* def);

    const Def* convert(const Def* def);
    const Def* flatten_ty(const Def* ty);
    const Def* flatten_ty_convert(const Def* ty);
    void aggregate_sigma(const Def* ty, DefQueue& ops);
    const Def* wrap(const Def* def, const Def* target_ty);
    const Def* reshape(const Def* mem, const Def* ty, DefQueue& vars);

    std::vector<ReshapePlugin> plugins_;
    Def2Def old2new_;
    Def2Def target_types_;
    std::stack<Lam*> worklist_;
    Mode mode_;
    Def2Def old2flatten_;
};

} // namespace thorin::mem
