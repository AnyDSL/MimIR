
#ifndef THORIN_UNTYPE_CLOSURES_H
#define THORIN_UNTYPE_CLOSURES_H

#include <queue>

#include "thorin/world.h"
#include "thorin/transform/closure_conv.h"

/// Convert from typed closuras (represented by <code>Σt.[t, cn[t, <args>]code>)
/// to untyped closures, where the environment is passed via memory (as <code>i8*</code>).
/// The following assumptions are made:
/// - @p Lam%s in callee-postion are λ-lifted and not closure converted (See @p Scalerize)
/// - each function receives its environment as its first paramter
/// - closure types have the aforementioned form, @see isa_ctype
/// 
/// All environments are heap allocated. External funtions receive <code>[]</code> as their
/// environment instead of a pointer

namespace thorin {


class LowerTypedClosures {
public:

    using StubQueue = std::queue<std::tuple<const Def*, const Def*, Lam*>>;

    LowerTypedClosures(World& world)
        : world_(world)
        , old2new_()
        , worklist_() 
        , dummy_ret_(world.bot(world.cn(world.type_mem()))) {}

    void run();

    const Def* env_type() {
        auto& w = world();
        return w.type_ptr(w.sigma());
    }

    bool unbox_env(const Def* type);

private:

    const Def* rewrite(const Def* def);


    enum Mode { Box = 0, Unbox, No_Env };
    Lam* make_stub(Lam* lam, enum Mode mode, bool adjust_bb_type);

    template<class D = const Def>
    D* map(const Def* old_def, D* new_def) {
        old2new_[old_def] = static_cast<const Def*>(new_def);
        return new_def;
    }

    World& world() {
        return world_;
    }

    World& world_;
    Def2Def old2new_;
    StubQueue worklist_;

    const Def* const dummy_ret_;
    const Def* lvm_;  //< Last visited memory token
    const Def* lcm_;  //< Last created memory token
};

}

#endif
