#pragma once

#include <string>

#include <thorin/def.h>
#include <thorin/pass/pass.h>

#include "dialects/mem/mem.h"

namespace thorin::autodiff {

const Def* create_ho_pb_type(const Def* def, const Def* arg_ty);

enum State { Unknown, Augment, Invert };

struct LoopData {
    const Def* index       = nullptr;
    const Def* cache_index = nullptr;
};

class AutoDiffEval;
struct LoopFrame {
    AutoDiffEval& autodiff_;
    LoopFrame(AutoDiffEval& autodiff)
        : autodiff_(autodiff) {}

    std::shared_ptr<LoopFrame> parent = nullptr;
    const Def* size                   = nullptr;
    const Def* local_size             = nullptr;

    LoopData forward;
    LoopData backward;

    const Def*& index() { return data().index; }

    const Def*& cache_index() { return data().cache_index; }

    LoopData& data();
};

struct InitFrame {
    Lam* lam;
    const Def* mem;
};

struct ShareFrame {
    DefVec items;
};

Lam* chain(const Def* mem, Lam* lam, const Def* next);
Lam* chain(const Def* first, const Def* second);
const Def* zero(World& w);
const Def* one(World& w);

/// This pass is the heart of AD.
/// We replace an `autodiff fun` call with the differentiated function.
class AutoDiffEval : public RWPass<AutoDiffEval, Lam> {
public:
    AutoDiffEval(PassMan& man)
        : RWPass(man, "autodiff_eval") {}

    /// Detect autodiff calls.
    const Def* rewrite(const Def*) override;

    /// Acts on toplevel autodiff on closed terms:
    /// * Replaces lambdas, operators with the appropriate derivatives.
    /// * Creates new lambda, calls associate variables, init maps, calls augment.
    const Def* derive(const Def*);
    const Def* derive_(const Def*);

    /// Applies to (open) expressions in a functional context.
    /// Returns the rewritten expressions and augments the partial and modular pullbacks.
    /// The rewrite is identity on the term up to renaming of variables.
    /// Otherwise, only pullbacks are added.
    /// To do so, some calls (e.g. axioms) are replaced by their derivatives.
    /// This transformation can be seen as an augmentation with a dual computation that generates the derivatives.
    const Def* augment(const Def*);
    const Def* augment_(const Def*);
    /// helper functions for augment
    const Def* augment_var(const Var*);
    const Def* augment_lam(Lam*);
    const Def* augment_extract(const Extract*);
    const Def* augment_app(const App*);
    const Def* augment_lit(const Lit*);
    const Def* augment_tuple(const Tuple*);
    const Def* augment_pack(const Pack* pack);
    const Def* augment_lea(const App*);
    const Def* augment_load(const App*);
    const Def* augment_store(const App*);
    const Def* augment_malloc(const App*);
    const Def* augment_alloc(const App*);
    const Def* augment_bitcast(const App*);
    const Def* augment_for(const App*);
    const Def* augment_for_body(const Def* body, const Def* start, const Def* inc);

    const Def* invert(const Def* def);
    const Def* invert_(const Def* def);

    const Def* invert_var(const Var*);
    const Def* invert_lam(Lam*);
    const Def* invert_extract(const Extract*);
    const Def* invert_app(const App*);
    const Def* invert_lit(const Lit*);
    const Def* invert_tuple(const Tuple*);
    const Def* invert_pack(const Pack* pack);
    const Def* invert_lea(const App*);
    const Def* invert_load(const App*);
    const Def* invert_store(const App*);
    const Def* invert_malloc(const App*);
    const Def* invert_alloc(const App*);
    const Def* invert_bitcast(const App*);
    const Def* invert_for(const App*);
    const Def* invert_for_body(const App* for_app);

    const Def* create_init_frame(const std::string& name, std::function<const Def*(const Def*)> func);
    const Def* create_init_alloc_frame(const std::string& name, const Def* alloc_ty);

    const Def* normalized_to_cache_index(const Def* normalized_index);

    const Def* op_load(const Def* ptr, bool with_mem = false);
    void op_store(const Def* ptr, const Def* value);
    const Def* op_slot(const Def* type, const std::string& name = "");
    void op_free(const Def* ptr);
    void ret(Lam* lam);

    void push_loop_frame(const App* for_app, const Def* size);
    void pop_loop_frame();

    const Def* resolve(const Def* def);
    void propagate(const Def* target, const Def* gradient);

    const Def* autodiff_zero(const Def* mem);
    const Def* autodiff_zero(const Def* mem, const Def* def);
    const Def* zero_pullback(const Def* domain);

    Lam* create_block(const std::string& name);
    Lam* create_return(const std::string& name);
    Lam* create_end(const std::string& name);

    Lam* create_invert_block(const std::string& name);
    Lam* create_invert_return(const std::string& name);
    Lam* create_invert_end();

    Lam* init_sinks();
    Lam* free_memory();

    void fetch_gradients(Lam* diff, Lam* backward);
    const Def* build_gradient_tuple(const Def* mem, Lam* diffee);

    const Def* wrap_cache(const App* load, const App* aug_load);
    const Def* get_pullback(const Def* op);
    Lam* free_memory_lam();
    const Def* augment_loop_body(const Def* body);

    void add_inverted(const Def* key, const Def* value) {
        assert(value);
        key->dump();
        inverted[key] = value;
    }

    void init_mem(const Def* mem) {
        assert(current_mem == nullptr);
        current_mem = mem;
    }

    void init_mem(Lam* lam) { init_mem(mem::mem_var(lam)); }

    const Def* end_mem() {
        auto mem    = current_mem;
        current_mem = nullptr;
        return mem;
    }

    friend LoopFrame;

private:
    /// Transforms closed terms (lambda, operator) to derived expressions.
    /// `f => f' = λ x. (f x, f*_x)`
    /// src Def -> dst Def
    Def2Def derived;
    /// Associates expressions (not necessarily closed) in a functional context to their derivatived counterpart.
    /// src Def -> dst Def
    Def2Def augmented;
    Def2Def inverted;

    /// dst Def -> dst Def
    Def2Def partial_pullback;

    /// Shadows the structure of containers for additional auxiliary pullbacks.
    /// A very advanced optimization might be able to recover shadow pullbacks from the partial pullbacks.
    /// Example: The shadow pullback of a tuple is a tuple of pullbacks.
    /// Shadow pullbacks are not modular (composable with other pullbacks).
    /// The structure pullback only preserves structure shallowly:
    /// A n-times nested tuple has a tuple of "normal" pullbacks as shadow pullback.
    /// Each inner nested tuples should have their own structure pullback by construction.
    /// ```
    /// e  : [B0, [B11, B12], B2]
    /// e* : [B0, [B11, B12], B2] -> A
    /// e*S: [B0 -> A, [B11, B12] -> A, B2 -> A]
    /// ```
    /// short theory of shadow pb:
    /// ```
    /// t: [B0, ..., Bn]
    /// t*: [B0, ..., Bn] -> A
    /// t*_S: [B0 -> A, ..., Bn -> A]
    /// b = t#i : Bi
    /// b* : Bi -> A
    /// b* = t*_S #i (if exists)
    /// ```
    /// This is equivalent to:
    /// `\lambda (s:Bi). t*_S (insert s at i in (zero [B0, ..., Bn]))`
    /// dst Def -> dst Def
    Def2Def shadow_pullback;

    // TODO: remove?
    Def2Def app_pb;

    Def2Def shadow_pullbacks;
    Def2Def gradient_ptrs;

    DefSet allocated_memory;
    DefSet caches;
    DefSet loads_;

    Def2Def cache_map;
    Def2Def index_map;
    Def2Def gradient_sinks;
    Def2Def sharing;
    DefMap<std::shared_ptr<LoopFrame>> cache_loop_assignment;
    DefMap<std::shared_ptr<LoopFrame>> loop_assignment;
    bool is_propagating;

    Lam* f;
    Lam* f_diff;

    // std::stack<LoopContext> loop_stack;

    std::stack<InitFrame> init_frames;
    std::shared_ptr<LoopFrame> current_loop = nullptr;

    const Def* current_mem = nullptr;
    State current_state    = State::Unknown;
};

} // namespace thorin::autodiff
