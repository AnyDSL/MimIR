#include "mim/analyses/domtree.h"

namespace mim {

// Cooper et al, 2001. A Simple, Fast Dominance Algorithm. http://www.cs.rice.edu/~keith/EMBED/dom.pdf
void DomTree::create() {
    idoms_[cfg().entry()] = cfg().entry();

    // all idoms different from entry are set to their first found dominating pred
    for (auto n : cfg().reverse_post_order().subspan(1)) {
        for (auto pred : n->preds()) {
            if (cfg().index(pred) < cfg().index(n)) {
                idoms_[n] = pred;
                goto outer_loop;
            }
        }
        fe::unreachable();
outer_loop:;
    }

    for (bool todo = true; todo;) {
        todo = false;

        for (auto n : cfg().reverse_post_order().subspan(1)) {
            const CFNode* new_idom = nullptr;
            for (auto pred : n->preds()) new_idom = new_idom ? least_common_ancestor(new_idom, pred) : pred;

            assert(new_idom);
            if (idom(n) != new_idom) {
                idoms_[n] = new_idom;
                todo      = true;
            }
        }
    }

    for (auto n : cfg().reverse_post_order().subspan(1)) children_[idom(n)].push_back(n);
}

void DomTree::depth(const CFNode* n, int i) {
    depth_[n] = i;
    for (auto child : children(n)) depth(child, i + 1);
}

const CFNode* DomTree::least_common_ancestor(const CFNode* i, const CFNode* j) const {
    assert(i && j);
    while (index(i) != index(j)) {
        while (index(i) < index(j)) j = idom(j);
        while (index(j) < index(i)) i = idom(i);
    }
    return i;
}

} // namespace mim
