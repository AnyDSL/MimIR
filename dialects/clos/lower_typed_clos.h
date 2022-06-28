#ifndef THORIN_UNTYPE_CLOSURES_H
#define THORIN_UNTYPE_CLOSURES_H

#include <queue>

#include "thorin/world.h"

#include "dialects/clos/clos_conv.h"
#include "dialects/mem/mem.h"

namespace thorin::clos {

/// This pass lowers *typed closures* to *untyped closures*.
/// For details on typed closures, see ClosConv.
/// In general, untyped closure have the form `(pointer-to-environment, code)` with the following exceptions:
/// - Lam%s in callee-position should be λ-lifted and thus don't receive an environment.
/// - External and imported (not set)  Lam%s also don't receive an environment (they are appropriately η-wrapped by
/// ClosConv)
/// - If the environment is of integer type, it's directly stored in the environment-pointer ("unboxed").
///   Note: In theory this should work for other primitive types as well, but the LL backend does not handle the
///   required conversion correctly.
///
/// Further, first class continuations are rewritten to returning functions. They receive `⊥` as a dummy continuation.
/// Therefore Clos2SJLJ should have taken place prior to this pass.
///
/// This pass will heap-allocate closures if they are annotated with ClosKind::esc and stack-allocate everything
/// else. These annotations are introduced by LowerTypedClosPrep.

class LowerTypedClos {
public:
    LowerTypedClos(World& world)
        : world_(world)
        , old2new_()
        , worklist_()
        , dummy_ret_(world.bot(world.cn(mem::type_mem(world)))) {}

    /// This runs the transformation.
    void run();

private:
    using StubQueue = std::queue<std::tuple<const Def*, const Def*, Lam*>>;

    /// Recursively rewrites a Def.
    const Def* rewrite(const Def* def);

    /// Describes how the environment should be treated.
    enum Mode {
        Box = 0, //< Environment is boxed (default).
        Unbox,   //< Environments is of primitive type (currently `iN`s) and directly stored in the pointer.
        No_Env   //< Lambda has no environment (lifted, top-level).
    };

    /// Create a new lambda stub.
    /// @p adjust_bb_type is true if the @p lam should be rewritten to a returning function.
    Lam* make_stub(Lam* lam, enum Mode mode, bool adjust_bb_type);

    /// @name helpers
    /// @{
    /// @name wrapper arround old2new_
    template<class D = const Def>
    D* map(const Def* old_def, D* new_def) {
        old2new_[old_def] = static_cast<const Def*>(new_def);
        return new_def;
    }

    World& world() { return world_; }

    /// Pointer type used to represent environments
    const Def* env_type() {
        auto& w = world();
        return mem::type_ptr(w.sigma());
    }
    /// @}

    World& world_;
    Def2Def old2new_;
    StubQueue worklist_;

    const Def* const dummy_ret_; //< dummy return continuation

    /// @name memory-tokens
    /// @{
    const Def* lvm_; //< Last visited memory token
    const Def* lcm_; //< Last created memory token
    /// @}
};

} // namespace thorin::clos

#endif