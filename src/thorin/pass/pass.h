#ifndef THORIN_PASS_PASS_H
#define THORIN_PASS_PASS_H

#include <stack>

#include "thorin/world.h"

#include "thorin/analyses/scope.h"

namespace thorin {

class PassMan;
using undo_t                    = size_t;
static constexpr undo_t No_Undo = std::numeric_limits<undo_t>::max();

/// This is a minimalistic base interface to work with when dynamically loading Pass%es.
class IPass {
public:
    IPass(PassMan& man, const char* name)
        : man_(man)
        , name_(name) {}

    virtual ~IPass() = default;

    PassMan& man() { return man_; }
    const PassMan& man() const { return man_; }
    const char* name() const { return name_; }

private:
    PassMan& man_;
    const char* name_;
};

using CreateIPass  = IPass* (*)(PassMan&);
using DestroyIPass = void (*)(IPass*);

/// All Passes that want to be registered in the PassMan must implement this interface.
/// * Directly inherit from this class if your pass doesn't need state and a fixed-point iteration (a ReWrite pass).
/// * Inherit from FPPass using CRTP if you do need state.
class Pass : public IPass {
public:
    Pass(PassMan& man, const char* name);
    virtual ~Pass() {}

    /// @name getters
    ///@{
    size_t index() const { return index_; }
    World& world();
    ///@}

    /// @name rewrite hook for the PassMan
    ///@{
    /// Rewrites a *structural* @p def within PassMan::curr_nom; returns the replacement.
    virtual const Def* rewrite(const Def* def) { return def; }
    virtual const Def* rewrite(const Var* var) { return var; }
    virtual const Def* rewrite(const Proxy* proxy) { return proxy; }
    ///@}

    /// @name analyze hook for the PassMan
    ///@{
    /// Invoked after the PassMan has finished Pass::rewrite%ing PassMan::curr_nom to analyze the Def;
    /// Will only be invoked if Pass::fixed_point() yields `true` - which will be the case for FPPass%es.
    /// @return No_Undo or the state to roll back to.
    virtual undo_t analyze(const Def*) { return No_Undo; }
    virtual undo_t analyze(const Var*) { return No_Undo; }
    virtual undo_t analyze(const Proxy*) { return No_Undo; }
    ///@}

    /// @name further hooks for the PassMan
    ///@{
    virtual bool fixed_point() const { return false; }

    /// Should the PassMan even consider this pass?
    virtual bool inspect() const = 0;

    /// Invoked just before Pass::rewrite%ing PassMan::curr_nom's body.
    virtual void enter() {}
    ///@}

    /// @name proxy
    ///@{
    const Proxy* proxy(const Def* type, Defs ops, flags_t flags = 0, const Def* dbg = {}) {
        return world().proxy(type, ops, index(), flags, dbg);
    }

    /// Check whether given @p def is a Proxy whose index matches this Pass's @p index.
    const Proxy* isa_proxy(const Def* def, flags_t flags = 0) {
        if (auto proxy = def->isa<Proxy>(); proxy != nullptr && proxy->index() == index() && proxy->flags() == flags)
            return proxy;
        return nullptr;
    }

    const Proxy* as_proxy(const Def* def, flags_t flags = 0) {
        auto proxy = def->as<Proxy>();
        assert(proxy->index() == index() && proxy->flags() == flags);
        return proxy;
    }
    ///@}

private:
    /// @name memory management
    ///@{
    virtual void* alloc() { return nullptr; }
    virtual void* copy(const void*) { return nullptr; }
    virtual void dealloc(void*) {}
    ///@}

    size_t index_;

    friend class PassMan;
};

/// An optimizer that combines several optimizations in an optimal way.
/// This is loosely based upon:
/// "Composing dataflow analyses and transformations" by Lerner, Grove, Chambers.
class PassMan {
public:
    PassMan(World& world)
        : world_(world) {}

    /// @name getters
    ///@{
    World& world() const { return world_; }
    const auto& passes() const { return passes_; }
    bool fixed_point() const { return fixed_point_; }
    Def* curr_nom() const { return curr_nom_; }
    ///@}

    /// @name create and run passes
    ///@{

    /// Add a pass to this PassMan.
    template<class P, class... Args>
    P* add(Args&&... args) {
        auto p   = std::make_unique<P>(*this, std::forward<Args>(args)...);
        auto res = p.get();
        fixed_point_ |= res->fixed_point();
        passes_.emplace_back(std::move(p));
        return res;
    }

    /// Run all registered passes on the whole World.
    void run();
    ///@}

private:
    /// @name state
    ///@{
    struct State {
        State()             = default;
        State(const State&) = delete;
        State(State&&)      = delete;
        State& operator=(State) = delete;
        State(size_t num)
            : data(num) {}

        Def* curr_nom = nullptr;
        DefArray old_ops;
        std::stack<Def*> stack;
        NomMap<undo_t> nom2visit;
        Array<void*> data;
        Def2Def old2new;
        DefSet analyzed;
    };

    void push_state();
    void pop_states(undo_t undo);
    State& curr_state() {
        assert(!states_.empty());
        return states_.back();
    }
    const State& curr_state() const {
        assert(!states_.empty());
        return states_.back();
    }
    undo_t curr_undo() const { return states_.size() - 1; }
    ///@}

    /// @name rewriting
    ///@{
    const Def* rewrite(const Def*);

    const Def* map(const Def* old_def, const Def* new_def) {
        curr_state().old2new[old_def] = new_def;
        curr_state().old2new.emplace(new_def, new_def);
        return new_def;
    }

    std::optional<const Def*> lookup(const Def* old_def) {
        for (auto& state : states_ | std::ranges::views::reverse)
            if (auto new_def = state.old2new.lookup(old_def)) return *new_def;
        return {};
    }
    ///@}

    /// @name analyze
    ///@{
    undo_t analyze(const Def*);
    bool analyzed(const Def* def) {
        for (auto& state : states_ | std::ranges::views::reverse) {
            if (state.analyzed.contains(def)) return true;
        }
        curr_state().analyzed.emplace(def);
        return false;
    }
    ///@}

    World& world_;
    std::vector<std::unique_ptr<Pass>> passes_;
    std::deque<State> states_;
    Def* curr_nom_    = nullptr;
    bool fixed_point_ = false;
    bool proxy_       = false;

    template<class P, class N>
    friend class FPPass;
};

template<class N = Def>
class RWPass : public Pass {
public:
    RWPass(PassMan& man, const char* name)
        : Pass(man, name) {}

    bool inspect() const override {
        if constexpr (std::is_same<N, Def>::value)
            return man().curr_nom();
        else
            return man().curr_nom()->template isa<N>();
    }

    N* curr_nom() const {
        if constexpr (std::is_same<N, Def>::value)
            return man().curr_nom();
        else
            return man().curr_nom()->template as<N>();
    }
};

/// Inherit from this class using CRTP if you do need a Pass with a state.
template<class P, class N = Def>
class FPPass : public RWPass<N> {
public:
    using Super = RWPass<N>;

    FPPass(PassMan& man, const char* name)
        : Super(man, name) {}

    bool fixed_point() const override { return true; }

    /// @name memory management for state
    ///@{
    void* alloc() override { return new typename P::Data(); } ///< Default ctor.
    void* copy(const void* p) override {
        return new typename P::Data(*static_cast<const typename P::Data*>(p));
    }                                                                                    ///< Copy ctor.
    void dealloc(void* state) override { delete static_cast<typename P::Data*>(state); } ///< Dtor.
    ///@}

protected:
    /// @name state-related getters
    ///@{
    auto& states() { return Super::man().states_; }
    auto& data() {
        assert(!states().empty());
        return *static_cast<typename P::Data*>(states().back().data[Super::index()]);
    }
    template<size_t I>
    auto& data() {
        assert(!states().empty());
        return std::get<I>(*static_cast<typename P::Data*>(states().back().data[Super::index()]));
    }
    /// Use this for your convenience if @c P::Data is a map.
    template<class K>
    auto& data(const K& key) {
        return data()[key];
    }
    /// Use this for your convenience if @c P::Data is a map.
    template<class K, size_t I>
    auto& data(const K& key) {
        return std::get<I>(data()[key]);
    }
    ///@}

    /// @name undo getters
    ///@{
    undo_t curr_undo() const { return Super::man().curr_undo(); }

    undo_t undo_visit(Def* nom) const {
        if (auto undo = Super::man().curr_state().nom2visit.lookup(nom)) return *undo;
        return No_Undo;
    }

    undo_t undo_enter(Def* nom) const {
        for (auto i = Super::man().states_.size(); i-- != 0;)
            if (Super::man().states_[i].curr_nom == nom) return i;
        return No_Undo;
    }
    ///@}
};

inline World& Pass::world() { return man().world(); }

} // namespace thorin

#endif
