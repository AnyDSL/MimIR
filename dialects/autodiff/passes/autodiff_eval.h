#pragma once

#include <string>
#include <vector>

#include <thorin/def.h>
#include <thorin/pass/pass.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
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

    DefMap<DefVec> gradients;

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
const Def* one(World& w);

struct Node;
using Nodes = std::unordered_set<Node*>;

struct Node {
    enum Type { Any = 0, Bot = 1, App = 2, For = 4, Branch = 8, Return = 16, Lam = 32 };

    Node(Type type)
        : type_(type) {}

    Type type_;
    Def* nom;
    const Def* def;
    Nodes preds;
    Nodes succs;
    mutable size_t index = -1;

    bool isa(Type type) { return type_ == Any || (type_ & type) != 0; }

    Node* pred() {
        assert(preds.size() == 1);
        return *(preds.begin());
    }

    Node* pred(Node::Type type) {
        for (auto& node : preds) {
            if (node->isa(type)) { return node; }
        }

        return nullptr;
    }
};

inline Node::Type operator|(Node::Type a, Node::Type b) {
    return static_cast<Node::Type>(static_cast<int>(a) | static_cast<int>(b));
}

class AutodiffAnalysis {
public:
    AutodiffAnalysis(Lam* entry) {
        exit_node_ = node(entry->ret_var());
        run(entry);
        entry_node_ = node(entry);
    }

    ~AutodiffAnalysis() {
        for (auto [key, value] : nodes_) { delete value; }
    }

    Node* node(const Def* def, Node::Type type = Node::Type::Bot) {
        Node* result = nodes_[def];
        if (!result) {
            result      = new Node(type);
            result->def = def;
            nodes_[def] = result;
        } else if (type != Node::Type::Bot && !result->isa(type)) {
            assert(result->isa(Node::Type::Bot));
            result->type_ = type;
        }

        return result;
    }

    Node* entry() { return entry_node_; }

    Node* exit() { return exit_node_; }

    size_t size() { return nodes_.size(); }

private:
    bool link(Node* left, Node* right) {
        auto size_before = left->succs.size();

        left->succs.insert(right);
        right->preds.insert(left);

        return size_before != left->succs.size();
    }

    void run(Lam* lam) {
        node(lam, Node::Type::Lam);
        run(lam, lam->body());
    }

    void run(const Def* prev, const Def* curr) {
        bool inserted = link(node(prev), node(curr));
        if (!inserted) return;
        run(curr);
    }

    void run(const Def* curr) {
        if (curr->isa<Var>()) return;

        if (auto lam = curr->isa_nom<Lam>()) {
            run(lam);
        } else if (auto app = curr->isa<App>()) {
            auto callee = app->callee();
            if (auto loop = match<affine::For>(curr)) {
                auto body = loop->arg(4);
                auto exit = loop->arg(5);
                node(loop, Node::Type::For);
                run(loop, body);
                run(loop, exit);
            } else if (callee_isa_var(callee)) {
                link(node(curr, Node::Type::App), node(callee));
            } else if (auto extract = callee->isa<Extract>()) {
                auto branches = extract->tuple();
                node(curr, Node::Type::Branch);
                for (auto branch : branches->projs()) { run(curr, branch); }
            } else if (auto next_lam = callee->isa_nom<Lam>()) {
                node(curr, Node::Type::App);
                run(curr, next_lam);
            } else {
                run(curr, app->arg());
                return;
            }

            run(app->arg());
        } else if (auto tup = curr->isa<Tuple>()) {
            for (auto proj : tup->projs()) { run(curr, proj); }
        } else if (auto pack = curr->isa<Pack>()) {
            run(curr, pack->body());
        } else if (auto extract = curr->isa<Extract>()) {
            curr->dump();
            run(curr, extract->tuple());
        } else {
        }
    }

    Node* entry_node_;
    Node* exit_node_;
    DefMap<Node*> nodes_;
};

class PostOrderVisitor {
    AutodiffAnalysis& analysis_;
    std::map<size_t, const Node*> nodes_;
    std::vector<const Node*> node_vec_;

public:
    PostOrderVisitor(AutodiffAnalysis& analysis)
        : analysis_(analysis) {}

    std::vector<const Node*>& post_order_visit(const Def* def) {
        nodes_.clear();
        auto node = analysis_.node(def);
        size_t i  = post_order_visit(node, analysis_.size());

        node_vec_.clear();
        for (auto [key, value] : nodes_) { node_vec_.push_back(value); }

        return node_vec_;
    }

private:
    size_t post_order_visit(const Node* n, size_t i) {
        auto& n_index = n->index;
        n_index       = size_t(-2);

        for (auto succ : n->succs) {
            if (!succ->isa(Node::Type::Bot)) continue;
            if (succ->index == size_t(-1)) i = post_order_visit(succ, i);
        }

        n_index = i - 1;
        nodes_.emplace(n_index, n);
        return n_index;
    }
};

/// This pass is the heart of AD.
/// We replace an `autodiff fun` call with the differentiated function.
class AutoDiffEval : public RWPass<AutoDiffEval, Lam> {
public:
    AutoDiffEval(PassMan& man)
        : RWPass(man, "autodiff_eval") {
    }

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
    const Def* augment_malloc(const App*);
    const Def* augment_alloc(const App*);
    const Def* augment_bitcast(const App*);
    const Def* augment_for(const App*);
    const Def* augment_for_body(const Def* body, const Def* start, const Def* inc);

    const Def* invert(const Def* def);
    const Def* invert_(const Def* def);

    const Def* invert_var(const Var*);
    Lam* invert_lam(Lam*);
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

    const Def* grad_arr(const Def* def);

    Lam* create_block(const std::string& name);
    Lam* create_lam(const Def* cn, const std::string& name);
    Lam* create_end(const std::string& name);

    Lam* free_memory();

    void fetch_gradients(Lam* diff, Lam* backward);

    const Def* wrap_cache(const App* load, const App* aug_load);

    const Def* grad_sum(const Def* def);
    const Def* grad_sum(const Def* def, const Def* default_zero_ty);
    void prop(Scope& scope, const Def* def);

    void add_inverted(const Def* key, const Def* value) {
        assert(value);
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
    Def2Def derived;
    Def2Def augmented;
    Def2Def inverted;
    DefSet visited_prop;

    Def2Def gradient_pointers;
    //DefMap<DefVec> gradients;

    DefSet allocated_memory;
    Def2Def cache_map;
    DefMap<std::shared_ptr<LoopFrame>> cache_loop_assignment;
    DefMap<std::shared_ptr<LoopFrame>> loop_assignment;

    std::stack<InitFrame> init_frames;

    std::shared_ptr<LoopFrame> root;
    std::shared_ptr<LoopFrame> current_loop = nullptr;

    const Def* current_mem = nullptr;
    State current_state    = State::Unknown;
};

} // namespace thorin::autodiff
