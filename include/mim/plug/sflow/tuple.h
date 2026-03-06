#pragma once

#include "mim/def.h"
#include "mim/lam.h"
#include "mim/tuple.h"

#include "mim/plug/sflow/autogen.h"

namespace mim::plug::sflow {

/// Extends mim::Dispatch with optional cont/merge fields for sflow.branch and sflow.loop.
/// - Regular dispatch: cont() and merge() return nullptr, kind() == Kind::None.
/// - sflow.branch {M} merge header: merge() returns the merge continuation, kind() == Kind::Branch.
/// - sflow.loop {C M} continue merge header: both cont() and merge() return the respective continuations, kind() ==
/// Kind::Loop.
class Dispatch : public mim::Dispatch {
public:
    enum class Kind { None, Branch, Loop };

    Dispatch(const Def* def)
        : mim::Dispatch(unwrap(def)) {
        if (auto br = Axm::isa<branch>(def)) {
            kind_                = Kind::Branch;
            auto [merge, header] = br->uncurry_args<2>();
            merge_               = merge;
        } else if (auto lp = Axm::isa<loop>(def)) {
            kind_                      = Kind::Loop;
            auto [cont, merge, header] = lp->uncurry_args<3>();
            cont_                      = cont;
            merge_                     = merge;
        }
    }

    /// The kind of dispatch (None, Branch, or Loop).
    Kind kind() const { return kind_; }

    /// The continue continuation (sflow.loop only), or nullptr.
    const Def* cont() const { return cont_; }

    /// The merge continuation (sflow.branch or sflow.loop), or nullptr.
    const Def* merge() const { return merge_; }

private:
    /// Unwraps sflow.branch/loop to extract the header (which is the dispatch).
    static const Def* unwrap(const Def* def) {
        if (auto br = Axm::isa<branch>(def)) {
            auto [merge, header] = br->uncurry_args<2>();
            return header;
        } else if (auto lp = Axm::isa<loop>(def)) {
            auto [cont, merge, header] = lp->uncurry_args<3>();
            return header;
        }
        return def;
    }

    Kind kind_        = Kind::None;
    const Def* cont_  = nullptr;
    const Def* merge_ = nullptr;
};

} // namespace mim::plug::sflow
