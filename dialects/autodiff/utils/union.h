#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/analysis/analysis.h"
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

using DefUnionNode = UnionNode<const Def*>;

} // namespace thorin::autodiff
