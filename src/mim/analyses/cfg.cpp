#include "mim/analyses/cfg.h"

#include "mim/world.h"

#include "mim/analyses/domfrontier.h"
#include "mim/analyses/domtree.h"
#include "mim/analyses/looptree.h"
#include "mim/analyses/scope.h"
#include "mim/util/util.h"

namespace mim {

//------------------------------------------------------------------------------

uint64_t CFNode::gid_counter_ = 0;

void CFNode::link(const CFNode* other) const {
    this->succs_.emplace(other);
    other->preds_.emplace(this);
}

std::ostream& operator<<(std::ostream& os, const CFNode* n) { return os << n->mut(); }

//------------------------------------------------------------------------------

CFA::CFA(const Scope& scope)
    : scope_(scope)
    , entry_(node(scope.entry()))
    , exit_(node(scope.exit())) {
    std::queue<Def*> cfg_queue;
    MutSet cfg_done;

    auto cfg_enqueue = [&](Def* mut) {
        if (mut->is_set() && cfg_done.emplace(mut).second) cfg_queue.push(mut);
    };

    cfg_enqueue(scope.entry());

    while (!cfg_queue.empty()) {
        auto src = pop(cfg_queue);
        std::queue<const Def*> queue;
        DefSet done;

        auto enqueue = [&](const Def* def) {
            if (def->isa<Var>()) return;
            // TODO maybe optimize a little bit by using the order
            if (scope.bound(def) && done.emplace(def).second) {
                if (auto dst = def->isa_mut()) {
                    cfg_enqueue(dst);
                    node(src)->link(node(dst));
                } else
                    queue.push(def);
            }
        };

        queue.push(src);

        while (!queue.empty()) {
            auto def = pop(queue);
            for (auto op : def->ops()) enqueue(op);
        }
    }

    link_to_exit();
    verify();
}

const CFNode* CFA::node(Def* mut) {
    auto&& n = nodes_[mut];
    if (n == nullptr) n = new CFNode(mut);
    return n;
}

CFA::~CFA() {
    for (const auto& p : nodes_) delete p.second;
}

const F_CFG& CFA::f_cfg() const { return lazy_init(this, f_cfg_); }
const B_CFG& CFA::b_cfg() const { return lazy_init(this, b_cfg_); }

void CFA::link_to_exit() {
    using CFNodeSet = mim::GIDSet<const CFNode*>;

    CFNodeSet reachable;
    std::queue<const CFNode*> queue;

    // first, link all nodes without succs to exit
    for (auto p : nodes()) {
        auto n = p.second;
        if (n != exit() && n->succs().empty()) n->link(exit());
    }

    auto backwards_reachable = [&](const CFNode* n) {
        auto enqueue = [&](const CFNode* n) {
            if (reachable.emplace(n).second) queue.push(n);
        };

        enqueue(n);

        while (!queue.empty())
            for (auto pred : pop(queue)->preds()) enqueue(pred);
    };

    std::stack<const CFNode*> stack;
    CFNodeSet on_stack;

    auto push = [&](const CFNode* n) {
        if (on_stack.emplace(n).second) {
            stack.push(n);
            return true;
        }

        return false;
    };

    backwards_reachable(exit());
    push(entry());

    while (!stack.empty()) {
        auto n = stack.top();

        bool todo = false;
        for (auto succ : n->succs()) todo |= push(succ);

        if (!todo) {
            if (!reachable.contains(n)) {
                n->link(exit());
                backwards_reachable(n);
            }

            stack.pop();
        }
    }
}

void CFA::verify() {
    bool error = false;
    for (const auto& p : nodes()) {
        auto in = p.second;
        if (in != entry() && in->preds_.size() == 0) {
            world().VLOG("missing predecessors: {}", in->mut());
            error = true;
        }
    }

    if (error) {
        // TODO
        assert(false && "CFG not sound");
    }
}

//------------------------------------------------------------------------------

template<bool forward>
CFG<forward>::CFG(const CFA& cfa)
    : cfa_(cfa)
    , rpo_(*this) {
    auto index = post_order_visit(entry(), size());
    assert_unused(index == 0);
}

template<bool forward> size_t CFG<forward>::post_order_visit(const CFNode* n, size_t i) {
    auto& n_index = forward ? n->f_index_ : n->b_index_;
    n_index       = size_t(-2);

    for (auto succ : succs(n))
        if (index(succ) == size_t(-1)) i = post_order_visit(succ, i);

    n_index = i - 1;
    rpo_[n] = n;
    return n_index;
}

// clang-format off
template<bool forward> const CFNodes& CFG<forward>::preds(const CFNode* n) const { assert(n != nullptr); return forward ? n->preds() : n->succs(); }
template<bool forward> const CFNodes& CFG<forward>::succs(const CFNode* n) const { assert(n != nullptr); return forward ? n->succs() : n->preds(); }
template<bool forward> const DomTreeBase<forward>& CFG<forward>::domtree() const { return lazy_init(this, domtree_); }
template<bool forward> const LoopTree<forward>& CFG<forward>::looptree() const { return lazy_init(this, looptree_); }
template<bool forward> const DomFrontierBase<forward>& CFG<forward>::domfrontier() const { return lazy_init(this, domfrontier_); }
// clang-format on

template class CFG<true>;
template class CFG<false>;

//------------------------------------------------------------------------------

} // namespace mim
