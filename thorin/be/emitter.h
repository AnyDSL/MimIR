#pragma once

#include "thorin/world.h"

#include "thorin/analyses/schedule.h"
#include "thorin/phase/phase.h"

namespace thorin {

template<class Value, class Type, class BB, class Child>
class Emitter : public ScopePhase {
private:
    constexpr const Child& child() const { return *static_cast<const Child*>(this); };
    constexpr Child& child() { return *static_cast<Child*>(this); };

    /// Integer wrapper for Emitter::emit that checks and retrieves/puts the `Value` from
    /// Emitter::locals_/Emitter::globals_.
    Value emit_(const Def* def) {
        auto place = scheduler_.smart(def);
        auto& bb   = lam2bb_[place->as_nom<Lam>()];
        return child().emit_bb(bb, def);
    }

public:
    Tab tab;

protected:
    Emitter(World& world, std::string_view name, std::ostream& ostream)
        : ScopePhase(world, name, false)
        , ostream_(ostream) {}

    std::ostream& ostream() const { return ostream_; }

    /// Recursively emits code.
    /// `mem`-typed @p def%s return sth that is `!child().is_valid(value)`.
    /// This variant asserts in this case.
    Value emit(const Def* def) {
        auto res = emit_unsafe(def);
        assert(child().is_valid(res));
        return res;
    }

    /// As above but returning `!child().is_valid(value)` is permitted.
    Value emit_unsafe(const Def* def) {
        if (auto i = globals_.find(def); i != globals_.end()) return i->second;
        if (auto i = locals_.find(def); i != locals_.end()) return i->second;

        auto val            = emit_(def);
        return locals_[def] = val;
    }

    void visit(const Scope& scope) override {
        if (entry_ = scope.entry()->isa_nom<Lam>(); !entry_) return;

        if (entry_->is_unset()) {
            child().emit_imported(entry_);
            return;
        }

        auto noms = schedule(scope); // TODO make sure to not compute twice

        // make sure that we don't need to rehash later on
        for (auto nom : noms) {
            if (auto lam = nom->isa<Lam>()) lam2bb_.emplace(lam, BB());
        }
        auto old_size = lam2bb_.size();

        entry_ = scope.entry()->as_nom<Lam>();
        assert(entry_->ret_var());

        auto fct = child().prepare(scope);

        Scheduler new_scheduler(scope);
        swap(scheduler_, new_scheduler);

        for (auto nom : noms) {
            if (auto lam = nom->isa<Lam>(); lam && lam != scope.exit()) {
                assert(lam == entry_ || lam->is_basicblock());
                child().emit_epilogue(lam);
            }
        }

        child().finalize(scope);
        locals_.clear();
        assert_unused(lam2bb_.size() == old_size && "really make sure we didn't triger a rehash");
    }

    std::ostream& ostream_;
    Scheduler scheduler_;
    DefMap<Value> locals_;
    DefMap<Value> globals_;
    DefMap<Type> types_;
    LamMap<BB> lam2bb_;
    Lam* entry_ = nullptr;
};

} // namespace thorin
