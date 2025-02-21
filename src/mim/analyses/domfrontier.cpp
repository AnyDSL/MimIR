#include "mim/analyses/domfrontier.h"

#include "mim/analyses/domtree.h"

namespace mim {

void DomFrontier::create() {
    const auto& domtree = cfg().domtree();
    for (auto n : cfg().reverse_post_order().subspan(1)) {
        const auto& preds = n->preds();
        if (preds.size() > 1) {
            auto idom = domtree.idom(n);
            for (auto pred : preds)
                for (auto i = pred; i != idom; i = domtree.idom(i)) link(i, n);
        }
    }
}

} // namespace mim
