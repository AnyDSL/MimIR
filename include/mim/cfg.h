#include <memory>

#include "mim/def.h"
#include "mim/lam.h"
#include "mim/tuple.h"

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
namespace mim {

class CFG {
public:
    class Node {
    public:
        const Lam* mut() { return mut_; }
        const auto& succs() const { return succs_; }
        const auto& preds() const { return preds_; }

    private:
        Node(CFG& cfg, const Lam* mut)
            : cfg_(cfg) {
            mut_ = mut;
        }

        void follow_escaping(const App* app);
        bool add_edge(const Lam* succ);
        void init();

        CFG& cfg_;
        const Lam* mut_;
        mutable const Node* idom_ = nullptr;
        // Nodes higher up in dominator tree within same sibling layer have higher postorder numbers.
        // This property is used to efficiently find the correct node for late code placement via [Nest::lca].
        mutable std::optional<size_t> postorder_number_ = std::nullopt;

        absl::flat_hash_set<Node*> succs_;
        absl::flat_hash_set<Node*> preds_;

        friend class CFG;
    };

    class Loop {
    public:
    private:
        absl::flat_hash_set<Node*> nodes_;
        absl::flat_hash_set<Node*> exits_;
    };

    CFG(Lam* entry, bool include_closed = false);

    void cfa();

    /// @name Nodes
    ///@{
    size_t num_nodes() const { return mut2node_.size(); }
    // clang-format off
    auto muts()  const { return mut2node_ | std::views::keys; }
    auto nodes() const { return mut2node_ | std::views::transform([](const auto& p) { return (const Node*)p.second.get(); }); }
    // clang-format on
    const Node* operator[](Def* mut) const { return (mut->isa<Lam>() ? (*this)[mut->as<Lam>()] : nullptr); }
    ///@}

private:
    Node* visit(const Lam* mut);

    void calc_dominance();

    Node* operator[](const Lam* mut) {
        if (auto i = mut2node_.find(mut); i != mut2node_.end()) return i->second.get();
        return nullptr;
    }

    World& world_;
    absl::flat_hash_map<Def*, std::unique_ptr<Node>> mut2node_;
    Node* entry_;
    bool include_closed_;
};

} // namespace mim
