#include "mim/plug/sflow/phase/structuralize.h"

#include "mim/tuple.h"

#include "mim/plug/sflow/autogen.h"
#include "mim/plug/sflow/phase/reduciblify.h"

#include "absl/container/flat_hash_set.h"

namespace mim::plug::sflow::phase {

void Structuralizer::visit(const Nest& nest) {
    auto root = nest.root();
    visit_node(root);
}

/// A node with at least two incoming forward edges is a merge node.
bool is_merge(const Nest::Node* node) {
    int count = 0;
    // Count forward edges towards node
    for (auto caller : node->sibl_deps<false>())
        if (caller->postorder_number() < node->postorder_number()) count++;
    return count > 1;
}

bool is_loop_header(const Nest::Node* header) {
    // Search for back edges
    for (auto node : header->sibl_deps<false>())
        if (node->postorder_number() < header->postorder_number()) return true;
    return false;
}

using ExitLabels = absl::flat_hash_set<u32>;
ExitLabels Structuralizer::visit_node(const Nest::Node* node) {
    auto mut = node->mut()->as_mut<Lam>();
    auto app = mut->body()->as<App>();

    ExitLabels exits;

    const Nest::Node* merge = nullptr;
    for (auto idomee : node->idomees()) {
        // Process idomee and collect exit labels
        exits.merge(visit_node(idomee));

        // Search for merge lam.
        // The merge lam of the structure is the idomee with highest postorder number.
        // TODO: for switch construct, lowest postorder number would be better, as fallthrough is allowed
        if (is_merge(idomee)) {
            if (!merge || idomee->postorder_number() > merge->postorder_number()) merge = idomee;
        }
    }

    if (!merge) {
        // No merge lam found, create new one.
        // TODO if no idomee is a merge node, all exits are multi-level
    }

    // Search for multi-level exits in idomees.
    // TODO: check local muts of idomees other than the merge mut for lams not dominated by the current node

    if (is_loop_header(node)) {
        // Leave irreducible loops untouched
        if (!Reduciblifier::is_reducible(node)) return ExitLabels{};

        // Find or create continue target
        const Def* continue_target = nullptr; // TODO

        if (auto callee = Lam::isa_mut_basicblock(app->callee())) {
            auto header
                = world().call<sflow::loop>(continue_target, merge->mut(), callee, world().lit_idx(1, 0), app->arg());
            mut->set_body(header);
        } else if (auto dispatch = app->isa<Dispatch>()) {
            auto header = world().call<sflow::loop>(continue_target, merge->mut(), dispatch->tuple(), dispatch->index(),
                                                    dispatch->arg());
            mut->set_body(header);
        }
    } else if (auto dispatch = app->isa<Dispatch>()) {
        auto header = world().call<sflow::select>(merge->mut(), dispatch->tuple(), dispatch->index(), dispatch->arg());
        mut->set_body(header);
    }

    return exits;
}

} // namespace mim::plug::sflow::phase
