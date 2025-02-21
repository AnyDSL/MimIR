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

CFG::CFG(const Scope& scope)
    : scope_(scope)
    , entry_(node(scope.entry())) {
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

    verify();

    rpo_       = std::make_unique<Map<const CFNode*>>(*this);
    auto index = post_order_visit(entry(), size());
    assert_unused(index == 0);
}

const CFNode* CFG::node(Def* mut) {
    auto&& n = nodes_[mut];
    if (n == nullptr) n = new CFNode(mut);
    return n;
}

CFG::~CFG() {
    for (const auto& p : nodes_) delete p.second;
}

void CFG::verify() {
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

size_t CFG::post_order_visit(const CFNode* n, size_t i) {
    auto& n_index = n->index_;
    n_index       = size_t(-2);

    for (auto succ : n->succs())
        if (index(succ) == size_t(-1)) i = post_order_visit(succ, i);

    n_index    = i - 1;
    (*rpo_)[n] = n;
    return n_index;
}

// clang-format off
const DomTree& CFG::domtree() const { return lazy_init(this, domtree_); }
const LoopTree& CFG::looptree() const { return lazy_init(this, looptree_); }
const DomFrontier& CFG::domfrontier() const { return lazy_init(this, domfrontier_); }
// clang-format on

//------------------------------------------------------------------------------

} // namespace mim
