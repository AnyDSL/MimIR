#pragma once

#include <deque>

#include "thorin/def.h"

namespace thorin {

class DepNode {
public:
    DepNode(Def* mut, size_t depth)
        : mut_(mut)
        , depth_(depth) {}

    Def* mut() const { return mut_; }
    size_t depth() const { return depth_; }
    DepNode* parent() const { return parent_; }
    const auto& children() const { return children_; }

private:
    DepNode* set_parent(DepNode* parent) {
        parent_ = parent;
        depth_  = parent->depth() + 1;
        parent->children_.emplace_back(this);
        return this;
    }

    Def* mut_;
    size_t depth_;
    DepNode* parent_ = nullptr;
    std::vector<DepNode*> children_;

    friend class DepTree;
};

class DepTree {
public:
    DepTree(World& world)
        : world_(world)
        , root_(std::make_unique<DepNode>(nullptr, 0)) {
        run();
    }

    World& world() const { return world_; };
    const DepNode* root() const { return root_.get(); }
    const DepNode* mut2node(Def* mut) const { return mut2node_.find(mut)->second.get(); }
    bool depends(Def* a, Def* b) const; ///< Does @p a depend on @p b?

private:
    void run();
    VarSet run(Def*);
    VarSet run(Def*, const Def*);
    static void adjust_depth(DepNode* node, size_t depth);

    World& world_;
    std::unique_ptr<DepNode> root_;
    MutMap<std::unique_ptr<DepNode>> mut2node_;
    DefMap<VarSet> def2vars_;
    std::deque<DepNode*> stack_;
};

} // namespace thorin
