#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
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

class PtrAnalysis {
public:
    std::unordered_map<const Def*, std::unique_ptr<UnionNode<const Def*>>> ptr_union;

    PtrAnalysis(Lam* lam) { run(lam); }

    const Def* representative(const Def* def) {
        auto node = ptr_node(def);
        return find(node)->value;
    }

    UnionNode<const Def*>* ptr_node(const Def* def) {
        auto i = ptr_union.find(def);
        if (i == ptr_union.end()) {
            auto p = ptr_union.emplace(def, std::make_unique<UnionNode<const Def*>>(def));
            assert_unused(p.second);
            i = p.first;
        }
        return &*i->second;
    }

    void run(Lam* lam) {
        Scope scope(lam);
        for (auto def : scope.bound()) {
            if (auto lea = match<mem::lea>(def)) {
                auto arg = lea->arg();
                auto arr = arg->proj(0);
                auto idx = arg->proj(1);

                unify(ptr_node(arr), ptr_node(lea));
            } else if (auto bitcast = match<core::bitcast>(def)) {
                auto ptr = bitcast->arg();
                unify(ptr_node(bitcast), ptr_node(ptr));
            }
        }
    }
};

} // namespace thorin::autodiff
