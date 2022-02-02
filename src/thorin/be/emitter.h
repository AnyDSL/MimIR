#ifndef THORIN_BE_EMITTER_H
#define THORIN_BE_EMITTER_H

namespace thorin {

template<class Value, class Type, class BB, class Child>
class Emitter {
private:
    constexpr const Child& child() const { return *static_cast<const Child*>(this); };
    constexpr Child& child() { return *static_cast<Child*>(this); };

    /// Internal wrapper for @p emit that checks and retrieves/puts the @c Value from @p defs_.
    Value emit_(const Def* def) {
        auto place = scheduler_.smart(def);
        auto& bb = lam2bb_[place->as_nom<Lam>()];
        return child().emit_bb(bb, def);
    }

protected:
    /// Recursively emits code. @c mem -typed @p Def%s return sth that is @c !child().is_valid(value) - this variant asserts in this case.
    Value emit(const Def* def) {
        auto res = emit_unsafe(def);
        assert(child().is_valid(res));
        return res;
    }

    /// As above but returning @c !child().is_valid(value) is permitted.
    Value emit_unsafe(const Def* def) {
        if (auto val = defs_.lookup(def)) return *val;
        if (auto lam = def->isa_nom<Lam>()) return defs_[lam] = child().emit_fun_decl(lam);

        auto val = emit_(def);
        return defs_[def] = val;
    }

    void emit_scope(const Scope& scope) {
        if (entry_ = scope.entry()->isa_nom<Lam>(); !entry_) return;

        auto noms = schedule(scope); // TODO make sure to not compute twice
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
    }

    Scheduler scheduler_;
    DefMap<Value> defs_;
    DefMap<Type> types_;
    LamMap<BB> lam2bb_;
    Lam* entry_ = nullptr;
};

}

#endif
