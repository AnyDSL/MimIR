#pragma once

#include <string>
#include <vector>

#include <thorin/def.h>
#include <thorin/pass/pass.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/analysis/analysis_factory.h"
#include "dialects/autodiff/analysis/cache_analysis.h"
#include "dialects/autodiff/analysis/gradient_analysis.h"
#include "dialects/autodiff/passes/def_inliner.h"
#include "dialects/autodiff/utils/helper.h"
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

    // DefMap<DefVec> gradients;
    DefSet allocated_memory;
    DefSet free_def;
    Def2Def gradient;
    DefMap<DefSet> cache2target;
    const Def* for_def = nullptr;

    void add_cache(const Def* cache, const Def* target) { cache2target[cache].insert(target); }

    DefMap<DefSet>& caches() { return cache2target; }
    const Def*& index() { return data().index; }
    const Def*& cache_index() { return data().cache_index; }

    LoopData& data();
};

struct InitFrame {
    Lam* lam;
    const Def* mem;
};

const Def* zero(const Def* ty);
const Def* zero(World& w);
const Def* one(const Def* ty);
const Def* one(World& w);

/// This pass is the heart of AD.
/// We replace an `autodiff fun` call with the differentiated function.
class AutoDiffEval : public RWPass<AutoDiffEval, Lam> {
public:
    AutoDiffEval(PassMan& man)
        : RWPass(man, "autodiff_eval") {}

    Lam* init_caches(Lam* next);

    const Def* rewrite(const Def*) override;

    const Def* derive(const Def*);
    const Def* derive_(const Def*);

    const Def* augment(const Def*);
    const Def* augment_(const Def*);
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
    const Def* augment_slot(const App*);
    const Def* augment_alloc(const App*);
    const Def* augment_bitcast(const App*);
    const Def* augment_for(const App*);
    const Def* augment_for_body(const Def* body, const Def* start, const Def* inc);

    const Def* invert(const Def* def);
    const Def* invert_(const Def* def);

    Lam* invert_lam(Lam*);
    Lam* invert_lam(AffineCFNode* node, const Def* ret_var);

    const Def* invert_var(const Var*);
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

    const Def* rewrite_rebuild(Rewriter& rewriter, const Def* def);

    void attach_gradient(const Def* dst, const Def* grad);

    const Def* create_init_frame(const std::string& name, std::function<const Def*(const Def*)> func);
    const Def* create_init_slot_frame(const std::string& name, const Def* alloc_ty, bool zero = false);
    const Def* create_init_alloc_frame(const std::string& name, const Def* alloc_ty, bool zero = false);

    const Def* normalized_to_cache_index(const Def* normalized_index);

    const Def* op_load_mem(const Def* ptr, const Def* dbg = {});
    const Def* op_load(const Def* ptr, const Def* dbg = {});
    const Def* op_store(const Def* ptr, const Def* value, const Def* dbg = {});
    const Def* op_slot_mem(const Def* type, const Def* dbg = {});
    const Def* op_slot(const Def* type, const Def* dbg = {});
    const Def* op_free(const Def* ptr, const Def* dbg = {});
    const Def* op_alloc_mem(const Def* type, const Def* dbg = {});
    const Def* op_alloc(const Def* type, const Def* dbg = {});
    void ret(Lam* lam);

    void push_loop_frame(const App* for_app, const Def* size);
    void pop_loop_frame();

    const Def* resolve(const Def* def);
    const Def* resolve_impl(const Def* def);

    const Def* grad_arr(const Def* def);
    void check_grad_arr(const Def* def);

    Lam* create_block(const std::string& name);
    Lam* create_lam(const Def* cn, const std::string& name);
    Lam* push_lam(const Def* cn, const std::string& name);

    Lam* free_memory();

    void assign_gradients(Lam* diffee, Lam* diff);
    const Def* input_mapping(Lam* forward);

    void preserve(const Def* target, const Def* value);

    const Def* get_gradient(const Def* def);
    const Def* get_gradient_or_default(const Def* def);
    void prop(const Def* def);

    const Def* upper_bound_size(const Def* size, bool lower = false);

    void add_inverted(const Def* key, const Def* value) {
        assert(value);
        inverted[key] = value;
    }

    void check_mem() { assert(current_mem != nullptr); }

    void push_mem(Lam* lam) { push_mem(mem::mem_var(lam)); }

    void push_mem(const Def* mem) {
        mem_stack.push(current_mem);
        current_mem = mem;
    }

    const Def* pop_mem() {
        auto last_mem = current_mem;
        assert(current_mem);
        auto top_mem = mem_stack.top();
        mem_stack.pop();
        current_mem = top_mem;
        return last_mem;
    }

    void build_branch_table(Lam* lam) {
        Scope scope(lam);
        auto& cfg = scope.f_cfg();
        for (auto node : cfg.reverse_post_order()) {
            size_t index = 0;
            for (auto pred : cfg.preds(node)) { branch_table[pred->nom()] = index++; }
            caller_count[node->nom()] = index;
        }
    }

    void push_scope(Lam* lam) { scope_stack.push(lam); }

    void pop_scope() { scope_stack.pop(); }

    Lam* current_scope() { return scope_stack.top(); }

    const Def* alias(const Def* def) { return factory->alias().get(def); }

    bool requires_caching(const Def* def) { return factory->cache().requires_caching(def); }
    DefSet& requires_loading(Lam* lam) { return factory->cache().requires_loading(lam); }

    bool has_gradient(const Def* def) { return factory->gradient().has_gradient(def); }

    size_t count_caller(const Def* def) { return caller_count[def]; }

    size_t branch_id_lit(const Def* def) {
        assert(branch_table.contains(def));
        return branch_table[def];
    }

    const Def* branch_id(const Def* def) { return world().lit_int(8, branch_id_lit(def)); }

    friend LoopFrame;

private:
    Def2Def derived;
    Def2Def augmented;
    Def2Def inverted;
    DefSet visited_prop;

    DefMap<size_t> branch_table;
    DefMap<size_t> caller_count;
    Def2Def gradient_pointers;

    Def2Def lam2branch;

    Def2Def cache_map;
    DefMap<Lam*> lam2inv;
    DefMap<std::shared_ptr<LoopFrame>> cache_loop_assignment;
    DefMap<std::shared_ptr<LoopFrame>> loop_assignment;
    DefMap<std::shared_ptr<LoopFrame>> index_loop_map;

    std::stack<InitFrame> init_frames;

    std::shared_ptr<LoopFrame> root;
    std::shared_ptr<LoopFrame> current_loop = nullptr;

    const Def* current_mem = nullptr;
    State current_state    = State::Unknown;
    std::stack<const Def*> mem_stack;
    std::stack<Lam*> scope_stack;

    std::unique_ptr<AnalysisFactory> factory;
};

} // namespace thorin::autodiff
