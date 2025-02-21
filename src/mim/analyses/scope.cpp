#include "mim/analyses/scope.h"

#include <fstream>

#include "mim/world.h"

#include "mim/analyses/cfg.h"
#include "mim/util/util.h"

namespace mim {

Scope::Scope(Def* entry)
    : world_(entry->world())
    , entry_(entry)
    , exit_(world().exit()) {
    run();
}

Scope::~Scope() {}

void Scope::run() {
    unique_queue<DefSet&> queue(bound_);

    if (auto var = entry_->has_var()) {
        queue.push(var);

        while (!queue.empty()) {
            for (auto use : queue.pop()->uses())
                if (use != entry_ && use != exit_) queue.push(use);
        }
    }
}

void Scope::calc_bound() const {
    if (has_bound_) return;
    has_bound_ = true;

    DefSet live;
    unique_queue<DefSet&> queue(live);

    auto enqueue = [&](const Def* def) {
        if (def == nullptr) return;
        if (def->dep_const()) return;

        if (bound_.contains(def))
            queue.push(def);
        else
            free_defs_.emplace(def);
    };

    for (auto op : entry()->partial_ops()) enqueue(op);

    while (!queue.empty())
        for (auto op : queue.pop()->partial_ops()) enqueue(op);

    swap(live, bound_);
}

void Scope::calc_free() const {
    if (has_free_) return;
    has_free_ = true;

    unique_queue<DefSet> queue;

    auto enqueue = [&](const Def* def) {
        if (def->dep_const()) return;

        if (auto var = def->isa<Var>())
            free_vars_.emplace(var);
        else if (auto mut = def->isa_mut())
            free_muts_.emplace(mut);
        else
            queue.push(def);
    };

    for (auto free : free_defs()) enqueue(free);

    while (!queue.empty())
        for (auto op : queue.pop()->extended_ops()) enqueue(op);
}

const CFA& Scope::cfa() const { return lazy_init(this, cfa_); }
const F_CFG& Scope::f_cfg() const { return cfa().f_cfg(); }
const B_CFG& Scope::b_cfg() const { return cfa().b_cfg(); }

bool Scope::is_free(Def* mut, const Def* def) {
    if (auto v = mut->var()) {
        if (auto var = v->isa<Var>()) {
            // optimize common cases first
            if (def->num_ops() == 0) return false;
            if (var == def) return true;
            for (auto v : var->mut()->tvars())
                if (v == def) return true;

            Scope scope(mut);
            return scope.bound(def);
        }
    }

    return false;
}

Nest::Nest(Def* r)
    : world_(r->world())
    , root_(make_node(r)) {
    std::queue<Def*> queue;
    queue.push(r);

    while (!queue.empty()) {
        auto curr = pop(queue);
        auto node = nodes_.find(curr)->second.get();
        for (auto op : curr->extended_ops()) {
            for (auto mut : op->local_muts()) {
                if (!(*this)[mut]) {
                    if (find_parent(mut, node)) queue.push(mut);
                }
            }
        }
    }
}

Nest::Node* Nest::make_node(Def* mut, Node* parent) {
    auto node = std::make_unique<Node>(mut, parent);
    auto res  = node.get();
    nodes_.emplace(mut, std::move(node));
    return res;
}

/// Tries to place @p mut as high as possible.
Nest::Node* Nest::find_parent(Def* mut, Node* begin) {
    for (Node* node = begin; node; node = node->parent()) {
        if (auto var = node->mut()->has_var()) {
            if (mut->free_vars().contains(var)) return make_node(mut, node);
        }
    }

    return nullptr;
}

Vars Nest::vars() const {
    if (!vars_) {
        auto vec = Vector<const Var*>();
        vec.reserve(num_nodes());
        for (const auto& [mut, _] : nodes_)
            if (auto var = mut->has_var()) vec.emplace_back(var);
        vars_ = world().vars().create(vec.begin(), vec.end());
    }

    return vars_;
}

void Nest::dot(const char* file) const {
    if (!file) {
        dot(std::cout);
    } else {
        auto of = std::ofstream(file);
        dot(of);
    }
}

void Nest::dot(std::ostream& os) const {
    Tab tab;
    (tab++).println(os, "digraph {{");
    tab.println(os, "ordering=out;");
    tab.println(os, "splines=false;");
    tab.println(os, "node [shape=box,style=filled];");
    root()->dot(tab, os);
    (--tab).println(os, "}}");
}

void Nest::Node::dot(Tab tab, std::ostream& os) const {
    for (const auto& [child, _] : children()) tab.println(os, "{} -> {}", mut()->unique_name(), child->unique_name());
    for (const auto& [_, child] : children()) child->dot(tab, os);
}

} // namespace mim
