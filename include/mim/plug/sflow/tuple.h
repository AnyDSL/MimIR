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
class Dispatch {
public:
    enum class Kind { None, Branch, Loop };

    Dispatch(const Def* def) {
        if (auto dispatch = def->isa<mim::Dispatch>()) {
            kind_  = Kind::None;
            tuple_ = dispatch->tuple();
            index_ = dispatch->index();
            arg_   = dispatch->arg();
        } else if (auto br = Axm::isa<branch>(def)) {
            auto [merge, tuple, index, arg] = br->uncurry_args<4>();

            kind_  = Kind::Branch;
            merge_ = merge->as_mut<Lam>();
            tuple_ = tuple;
            index_ = index;
            arg_   = arg;
        } else if (auto lp = Axm::isa<loop>(def)) {
            auto [cont, merge, tuple, index, arg] = lp->uncurry_args<5>();

            kind_  = Kind::Loop;
            cont_  = cont->as_mut<Lam>();
            merge_ = merge->as_mut<Lam>();
            tuple_ = tuple;
            index_ = index;
            arg_   = arg;
        }
    }

    explicit operator bool() const noexcept { return tuple_; }

    /// The kind of dispatch (None, Branch, or Loop).
    Kind kind() const { return kind_; }

    size_t num_targets() { return tuple_->num_ops(); }

    const Def* target(size_t i) const { return tuple()->proj(i); }

    const Def* tuple() const { return tuple_; }

    const Def* index() const { return index_; }

    const Def* arg() const { return arg_; }

    /// The continue continuation (sflow.loop only), or nullptr.
    Lam* cont() const { return cont_; }

    /// The merge continuation (sflow.branch or sflow.loop), or nullptr.
    Lam* merge() const { return merge_; }

private:
    Kind kind_        = Kind::None;
    const Def* tuple_ = nullptr;
    const Def* index_ = nullptr;
    const Def* arg_   = nullptr;
    Lam* cont_        = nullptr;
    Lam* merge_       = nullptr;
};

} // namespace mim::plug::sflow
