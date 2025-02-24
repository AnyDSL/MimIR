#pragma once

#include <memory>

#include "mim/analyses/nest.h"
#include "mim/util/indexmap.h"
#include "mim/util/indexset.h"

namespace mim {

class CFNode;
class LoopTree;

/// @name Control Flow
///@{
using CFNodes = GIDSet<const CFNode*>;
///@}

/// A Control-Flow Node.
/// Managed by CFG.
class CFNode {
public:
    CFNode(Def* mut)
        : mut_(mut)
        , gid_(gid_counter_++) {}

    uint64_t gid() const { return gid_; }
    size_t index() const { return index_; }
    Def* mut() const { return mut_; }
    const CFNodes& preds() const { return preds_; }
    const CFNodes& succs() const { return succs_; }
    size_t num_preds() const { return preds().size(); }
    size_t num_succs() const { return succs().size(); }

private:
    void link(const CFNode* other) const;

    mutable size_t index_ = -1; ///< RPO index in a CFG.

    Def* mut_;
    size_t gid_;
    static uint64_t gid_counter_;
    mutable CFNodes preds_;
    mutable CFNodes succs_;

    friend class CFG;
    friend std::ostream& operator<<(std::ostream&, const CFNode*);
};

//------------------------------------------------------------------------------

/// A Control-Flow Graph.
class CFG {
public:
    template<class Value> using Map = IndexMap<CFG, const CFNode*, Value>;
    using Set                       = IndexSet<CFG, const CFNode*>;

    CFG(const CFG&)     = delete;
    CFG& operator=(CFG) = delete;

    explicit CFG(const Nest&);
    ~CFG();

    World& world() const { return nest().world(); }
    const Nest& nest() const { return nest_; }
    const auto& nodes() const { return nodes_; }
    size_t size() const { return nodes_.size(); }
    const CFNode* entry() const { return entry_; }
    auto reverse_post_order() const { return rpo_->array().view(); }
    auto post_order() const { return std::views::reverse(rpo_->array()); }
    /// Maps from reverse post-order index to CFNode.
    const CFNode* reverse_post_order(size_t i) const { return rpo_->array()[i]; }
    /// Maps from post-order index to CFNode.
    const CFNode* post_order(size_t i) const { return rpo_->array()[size() - 1 - i]; }
    const CFNode* operator[](Def* mut) const {
        auto i = nodes_.find(mut);
        return i == nodes_.end() ? nullptr : i->second;
    }
    const LoopTree& looptree() const;

    static size_t index(const CFNode* n) { return n->index_; }

private:
    const CFNode* node(Def*);
    size_t post_order_visit(const CFNode* n, size_t i);
    void verify();

    const Nest& nest_;
    MutMap<const CFNode*> nodes_;
    const CFNode* entry_;
    std::unique_ptr<Map<const CFNode*>> rpo_;
    mutable std::unique_ptr<const LoopTree> looptree_;
};

//------------------------------------------------------------------------------

} // namespace mim
