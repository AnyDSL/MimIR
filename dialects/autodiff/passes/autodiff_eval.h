#pragma once

#include <string>
#include <vector>

#include <thorin/def.h>
#include <thorin/pass/pass.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/auxiliary/analyses_collection.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/auxiliary/autodiff_cache_analysis.h"
#include "dialects/autodiff/auxiliary/autodiff_flow_analysis.h"
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
    const Def* for_def = nullptr;

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
    const Def* augment_malloc(const App*);
    const Def* augment_alloc(const App*);
    const Def* augment_bitcast(const App*);
    const Def* augment_for(const App*);
    const Def* augment_for_body(const Def* body, const Def* start, const Def* inc);

    const Def* invert(const Def* def);
    const Def* invert_(const Def* def);

    const Def* invert_var(const Var*);
    Lam* invert_lam(Lam*);
    Lam* invert_lam(Node* call, const Def* ret_var);
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
    std::tuple<Lam*, Lam*> invert_for_body(const App* for_app);

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
    const Def* op_memset(const Def* ptr, const Def* dbg = {});
    void ret(Lam* lam);

    void push_loop_frame(const App* for_app, const Def* size);
    void pop_loop_frame();

    const Def* resolve(const Def* def);
    const Def* resolve_impl(const Def* def);

    const Def* grad_arr(const Def* def);
    void check_grad_arr(const Def* def);

    Lam* create_block(const std::string& name);
    Lam* create_ret(const Lam* lam);
    Lam* create_lam(const Def* cn, const std::string& name);
    Lam* create_end(const std::string& name);

    Lam* free_memory();

    const Def* fetch_gradients(Lam* backward);

    void assign_gradients(Lam* diffee, Lam* diff);
    const Def* input_mapping(Lam* forward);

    void preserve(const Def* value);

    const Def* get_gradient(const Def* def);
    const Def* get_gradient(const Def* def, const Def* default_zero_ty);
    void prop(Scope& scope, const Def* def);

    const Def* upper_bound_size(const Def* size, bool lower = false);

    void add_inverted(const Def* key, const Def* value) {
        assert(value);
        inverted[key] = value;
    }

    void check_mem() { assert(current_mem != nullptr); }
    /*
        void init_mem(const Def* mem) {
            assert(current_mem == nullptr);
            current_mem = mem;
        }

        void init_mem(Lam* lam) { init_mem(mem::mem_var(lam)); }*/

    void push_mem(Lam* lam) { push_mem(mem::mem_var(lam)); }

    void push_mem(const Def* mem) {
        mem_stack.push(current_mem);
        current_mem = mem;
    }

    const Def* pop_mem() {
        auto last_mem = current_mem;
        auto top_mem  = mem_stack.top();
        mem_stack.pop();
        current_mem = top_mem;
        return last_mem;
    }
    /*
        const Def* end_mem() {
            check_mem();
            auto mem    = current_mem;
            current_mem = nullptr;
            return mem;
        }*/

    void push_scope(Lam* lam) { scope_stack.push(lam); }

    void pop_scope() { scope_stack.pop(); }

    Lam* current_scope() { return scope_stack.top(); }

    bool requires_caching(const Def* def) { return analyses->cache().requires_caching(def); }

    bool isa_flow_def(const Def* def) { return analyses->flow().isa_flow_def(def); }

    size_t count_caller(const Def* def) { return analyses->dep().count_caller(def); }

    const Def* branch_index(const Def* def) {
        size_t index = analyses->dep().branch_index(def);
        return world().lit_int(8, index);
    }

    friend LoopFrame;

private:
    Def2Def derived;
    Def2Def augmented;
    Def2Def inverted;
    DefSet visited_prop;
    DefSet visited_scan;
    DefSet markings;

    Def2Def gradient_pointers;

    Def2Def cache_map;
    DefMap<Lam*> lam2inv;
    Def2Def lam2branch;
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

    std::unique_ptr<AnalysesCollection> analyses;
};

} // namespace thorin::autodiff
