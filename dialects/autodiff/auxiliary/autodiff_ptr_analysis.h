#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/auxiliary/analysis.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

template<class T>
struct UnionNode {
    UnionNode(T value)
        : parent(this)
        , value(value) {}

    bool is_root() const { return parent != nullptr; }
    UnionNode<T>* parent;
    T value;
};

template<class T>
UnionNode<T>* unify(UnionNode<T>* x, UnionNode<T>* y) {
    assert(x->is_root() && y->is_root());

    if (x == y) return x;
    return y->parent = x;
}

template<class T>
UnionNode<T>* find(UnionNode<T>* node) {
    if (node->parent != node) { node->parent = find(node->parent); }
    return node->parent;
}

class AnalysisFactory;
class PtrAnalysis : public Analysis {
public:
    std::unordered_map<const Def*, std::unique_ptr<UnionNode<const Def*>>> ptr_union;

    PtrAnalysis(AnalysisFactory& factory);

    const Def* representative(const Def* def) {
        auto node = ptr_node(def);
        return find(node)->value;
    }

    UnionNode<const Def*>* ptr_node(const Def* def);

    void run();
};

} // namespace thorin::autodiff
