#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/auxiliary/autodiff_flow_analysis.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

struct Node;
using Nodes = std::unordered_set<Node*>;

struct Node {
    enum Type { Any = 0, Bot = 1, App = 2, For = 4, Branch = 8, Return = 16, Lam = 32 };

    Node(const Def* def, Type type, size_t depth)
        : def(def)
        , type_(type)
        , depth(depth) {}

    Type type_;
    const Def* def;
    Nodes preds_;
    Nodes succs_;
    size_t depth;

    mutable size_t index = -1;

    bool isa(Type type) const { return type_ == Any || (type_ & type) != 0; }

    Node* pred() {
        assert(preds_.size() == 1);
        return *(preds_.begin());
    }

    Node* pred(Node::Type type) {
        for (auto& node : preds_) {
            if (node->isa(type)) { return node; }
        }

        return nullptr;
    }

    std::vector<Node*> preds(Node::Type type) {
        std::vector<Node*> result;
        for (auto& node : preds_) {
            if (node->isa(type)) { result.push_back(node); }
        }

        return result;
    }

    size_t count(Node::Type type) {
        size_t count = 0;
        for (auto& node : preds_) {
            if (node->isa(type)) count++;
        }

        return count;
    }
};

inline Node::Type operator|(Node::Type a, Node::Type b) {
    return static_cast<Node::Type>(static_cast<int>(a) | static_cast<int>(b));
}

class PostOrderVisitor;
class DepAnalysis {
public:
    Node* entry_node_;
    Node* exit_node_;
    DefMap<Node*> nodes_;
    DefMap<size_t> branch_table;
    size_t depth           = 0;
    bool branch_table_init = false;

    DepAnalysis(Lam* entry) {
        if (auto ret_var = entry->ret_var()) { exit_node_ = create(ret_var); }
        run(entry);
        entry_node_ = create(entry);
    }

    ~DepAnalysis() {
        for (auto [key, value] : nodes_) { delete value; }
    }

    size_t count_caller(const Def* def) {
        auto def_node = node(def);
        if (!def_node) return 0;
        return def_node->count(Node::Type::App | Node::Type::For | Node::Type::Branch);
    }

    size_t branch_index(const Def* def) {
        if (!branch_table_init) {
            branch_table_init = true;
            build_branch_table();
        }
        assert(branch_table.contains(def));
        return branch_table[def];
    }

    void build_branch_table() {
        DefSet visited;
        build_branch_table(exit(), visited);
    }

    void build_branch_table(Node* node, DefSet& visited) {
        if (!visited.insert(node->def).second) return;
        size_t index = 0;
        for (auto pred : node->preds(Node::Type::App | Node::Type::For | Node::Type::Branch)) {
            auto caller = pred->pred(Node::Type::Lam);
            if (branch_table.contains(caller->def)) return;
            branch_table[caller->def] = index++;
            build_branch_table(caller, visited);
        }
    }

    Node* node(const Def* def, Node::Type type = Node::Type::Bot) { return nodes_[def]; }

    Node* create(const Def* def, Node::Type type = Node::Type::Bot) {
        Node* result = nodes_[def];
        if (!result) {
            result      = new Node(def, type, depth);
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
        auto size_before = left->succs_.size();

        left->succs_.insert(right);
        right->preds_.insert(left);

        return size_before != left->succs_.size();
    }

    void run(Lam* lam) {
        create(lam, Node::Type::Lam);
        run(lam, lam->body());
    }

    void run(const Def* prev, const Def* curr) {
        bool inserted = link(create(prev), create(curr));
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
                create(loop, Node::Type::For);
                depth++;
                run(loop, body);
                depth--;
                run(loop, exit);
            } else if (callee_isa_var(callee)) {
                link(create(curr, Node::Type::App), create(callee));
            } else if (auto extract = callee->isa<Extract>()) {
                auto branches = extract->tuple();
                create(curr, Node::Type::Branch);
                for (auto branch : branches->projs()) { run(curr, branch); }
            } else if (auto next_lam = callee->isa_nom<Lam>()) {
                create(curr, Node::Type::App);
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
            run(curr, extract->tuple());
        } else {
        }
    }

    friend PostOrderVisitor;
};

class PostOrderVisitor {
    DepAnalysis& analysis_;
    std::map<size_t, const Node*> nodes_;
    std::vector<const Node*> node_vec_;

    size_t last_index = -1;

public:
    PostOrderVisitor(DepAnalysis& analysis)
        : analysis_(analysis) {
        for (auto [_, node] : analysis.nodes_) { add(node); }
    }

    void add(const Def* def) { add(analysis_.node(def)); }

    void add(Node* node) {
        if (!node) return;
        last_index = post_order_visit(node, last_index);
    }

    void begin() {
        nodes_.clear();
        for (auto [def, node] : analysis_.nodes_) { node->index = -1; }
        last_index = analysis_.size();
    }

    std::vector<const Node*>& post_order_visit() {
        node_vec_.clear();
        for (auto [key, value] : nodes_) { node_vec_.push_back(value); }
        return node_vec_;
    }

private:
    size_t post_order_visit(const Node* n, size_t i) {
        auto& n_index = n->index;
        if (n_index != size_t(-1)) return i;
        n_index = size_t(-2);

        for (auto succ : n->succs_) {
            if (!succ->isa(Node::Type::Bot)) { continue; }
            i = post_order_visit(succ, i);
        }

        n_index = i - 1;
        nodes_.emplace(n_index, n);
        return n_index;
    }
};

} // namespace thorin::autodiff
