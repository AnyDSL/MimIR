#ifndef THORIN_BE_EMITTER_H
#define THORIN_BE_EMITTER_H

#include "thorin/world.h"

#include "thorin/analyses/schedule.h"

namespace thorin {

template<class Value, class Type, class BB, class Child>
class Emitter {
private:
    constexpr const Child& child() const { return *static_cast<const Child*>(this); };
    constexpr Child& child() { return *static_cast<Child*>(this); };

    /// Internal wrapper for @p emit that checks and retrieves/puts the @c Value from @p locals_/@p globals_.
    Value emit_(const Def* def) {
        auto place = scheduler_.smart(def);
        auto& bb   = lam2bb_[place->as_nom<Lam>()];
        return child().emit_bb(bb, def);
    }

protected:
    Emitter(World& world, Stream& stream)
        : world_(world)
        , stream_(stream) {}

    World& world() const { return world_; }
    const Scope& scope() const { return *scope_; }
    Stream& stream() const { return stream_; }

    /// Recursively emits code. @c mem -typed @p Def%s return sth that is @c !child().is_valid(value) - this variant
    /// asserts in this case.
    Value emit(const Def* def) {
        auto res = emit_unsafe(def);
        assert(child().is_valid(res));
        return res;
    }

    /// As above but returning @c !child().is_valid(value) is permitted.
    Value emit_unsafe(const Def* def) {
        if (auto val = globals_.lookup(def)) return *val;
        if (auto val = locals_.lookup(def)) return *val;

        auto val            = emit_(def);
        return locals_[def] = val;
    }

    void emit_module() {
        world().template visit<false>([&](const Scope& scope) { emit_scope(scope); });
    }

    void emit_scope(const Scope& scope) {
        if (entry_ = scope.entry()->isa_nom<Lam>(); !entry_) return;
        scope_ = &scope;

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
        assert(lam2bb_.size() == old_size && "really make sure we didn't triger a rehash");
    }

    World& world_;
    const Scope* scope_ = nullptr;
    Stream& stream_;
    Scheduler scheduler_;
    DefMap<Value> locals_;
    DefMap<Value> globals_;
    DefMap<Type> types_;
    LamMap<BB> lam2bb_;
    Lam* entry_ = nullptr;
};

} // namespace thorin

#endif
