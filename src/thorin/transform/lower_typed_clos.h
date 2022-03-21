
#ifndef THORIN_UNTYPE_CLOSURES_H
#define THORIN_UNTYPE_CLOSURES_H

#include <queue>

#include "thorin/world.h"
#include "thorin/transform/clos_conv.h"


namespace thorin {

    /// @brief This pass lowers *typed closures* to *untyped closures*.
    /// For details on typed closures, see @p ClosConv.
    /// In general, untyped closure have the form `(pointer-to-environment, code)`, with the following exceptions:
    /// - @p Lam%s in callee-postion should be λ-lifted and thus don't receive an environment.
    /// - external and imported (not set) @p Lam%s also don't receive an environment (they are appropriatly η-wrapped by @p ClosConv)
    /// - if the environment is of integer type, it's directly stored in the environment-part. 
    //    (This should work for other primitve types as well, but the LL backend does not handle required conversion correctly).
    ///  
    ///
    /// Futher, first class continuations are rewritten to returning functions: they receive ⊥ as a dummy continuation.
    /// Therefore @p Clos2SJLJ should have taken place prior to this pass.
    ///
    /// This pass will heap-allocate closures if they are annotated with @p ClosKind::escaping and stack-allocate them otherwise.
    /// These annotations are introduced by @p LowerTypedClosPrep.

class LowerTypedClos {
public:


    LowerTypedClos(World& world)
        : world_(world)
        , old2new_()
        , worklist_() 
        , dummy_ret_(world.bot(world.cn(world.type_mem()))) {}

    /// @brief perform the transformation
    void run();


private:

    using StubQueue = std::queue<std::tuple<const Def*, const Def*, Lam*>>;

    /// @brief recursively rewrites a @p Def
    const Def* rewrite(const Def* def);

    /// @brief Describes how the environment should be treated
    enum Mode { 
        Box = 0,  //< Box environment (default)
        Unbox,    //< Unbox environments of primitve type (currently `iN`s)
        No_Env    //< Lambda has no environment (lifted, toplevel)
    };

    /// @brief create a new lambda stub
    /// @param adjust_bb_type is true if the @p lam should be rewritten to a returning function.
    Lam* make_stub(Lam* lam, enum Mode mode, bool adjust_bb_type);

    /// @name helpers
    /// @{
    /// @name wrapper arround old2new_
    template<class D = const Def>
    D* map(const Def* old_def, D* new_def) {
        old2new_[old_def] = static_cast<const Def*>(new_def);
        return new_def;
    }

    World& world() {
        return world_;
    }

    /// @brief pointer type used to represent environments
    const Def* env_type() {
        auto& w = world();
        return w.type_ptr(w.sigma());
    }
    /// @}

    World& world_;
    Def2Def old2new_;
    StubQueue worklist_;

    const Def* const dummy_ret_; //< dummy return continuation

    /// @name memory-tokens
    /// @{
    const Def* lvm_;  //< Last visited memory token
    const Def* lcm_;  //< Last created memory token
    /// @}
};

}

#endif
