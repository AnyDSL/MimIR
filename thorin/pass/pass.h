#pragma once

#include <stack>
#include <typeindex>

#include "thorin/world.h"

namespace thorin {

class PassMan;
using undo_t                    = size_t;
static constexpr undo_t No_Undo = std::numeric_limits<undo_t>::max();

/// All Pass%es that want to be registered in the PassMan must implement this interface.
/// * Inherit from RWPass if your pass does **not** need state and a fixed-point iteration.
/// * Inherit from FPPass if you **do** need state and a fixed-point.
/// * If you do not rely on interaction between differen Pass%es, consider using Phase instead.
class Pass {
public:
    Pass(PassMan&, std::string_view name);
    virtual ~Pass() = default;

    /// @name Setters
    ///@{
    World& world();
    PassMan& man() { return man_; }
    const PassMan& man() const { return man_; }
    std::string_view name() const { return name_; }
    size_t index() const { return index_; }
    ///@}

    /// @name Rewrite Hook for the PassMan
    ///@{
    /// Rewrites an *immutable* @p def within PassMan::curr_mut.
    /// @returns the replacement.
    virtual Ref rewrite(Ref def) { return def; }
    virtual Ref rewrite(const Var* var) { return var; }
    virtual Ref rewrite(const Proxy* proxy) { return proxy; }
    ///@}

    /// @name Analyze Hook for the PassMan
    ///@{
    /// Invoked after the PassMan has finished Pass::rewrite%ing PassMan::curr_mut to analyze the Def.
    /// Will only be invoked if Pass::fixed_point() yields `true` - which will be the case for FPPass%es.
    /// @returns thorin::No_Undo or the state to roll back to.
    virtual undo_t analyze(Ref) { return No_Undo; }
    virtual undo_t analyze(const Var*) { return No_Undo; }
    virtual undo_t analyze(const Proxy*) { return No_Undo; }
    ///@}

    /// @name Further Hooks for the PassMan
    ///@{
    virtual bool fixed_point() const { return false; }

    /// Should the PassMan even consider this pass?
    virtual bool inspect() const = 0;

    /// Invoked just before Pass::rewrite%ing PassMan::curr_mut's body.
    /// @note This is invoked when seeing the *inside* of a mutable the first time.
    /// This is often too late, as you usually want to do something when you see a mutable the first time from the
    /// *outside*. This means that this PassMan::curr_mut has already been encountered elsewhere. Otherwise, we wouldn't
    /// have seen PassMan::curr_mut to begin with (unless it is Def::is_external).
    virtual void enter() {}

    /// Invoked **once** before entering the main rewrite loop.
    virtual void prepare() {}
    ///@}

    /// @name proxy
    ///@{
    const Proxy* proxy(Ref type, Defs ops, u32 tag = 0) { return world().proxy(type, ops, index(), tag); }
    /// Check whether given @p def is a Proxy whose Proxy::pass matches this Pass's @p IPass::index.
    const Proxy* isa_proxy(Ref def, u32 tag = 0) {
        if (auto proxy = def->isa<Proxy>(); proxy != nullptr && proxy->pass() == index() && proxy->tag() == tag)
            return proxy;
        return nullptr;
    }
    const Proxy* as_proxy(Ref def, u32 tag = 0) {
        auto proxy = def->as<Proxy>();
        assert_unused(proxy->pass() == index() && proxy->tag() == tag);
        return proxy;
    }
    ///@}

private:
    /// @name Memory Management
    ///@{
    virtual void* alloc() { return nullptr; }           ///< Default constructor.
    virtual void* copy(const void*) { return nullptr; } ///< Copy constructor.
    virtual void dealloc(void*) {}                      ///< Destructor.
    ///@}

    PassMan& man_;
    std::string name_;
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

    /// @name Getters
    ///@{
    World& world() const { return world_; }
    const auto& passes() const { return passes_; }
    bool fixed_point() const { return fixed_point_; }
    Def* curr_mut() const { return curr_mut_; }
    ///@}

    /// @name Create and run Passes
    ///@{
    void run(); ///< Run all registered passes on the whole World.

    /// Add a pass to this PassMan.
    /// If a pass of the same class has been added already, returns the earlier added instance.
    template<class P, class... Args>
    P* add(Args&&... args) {
        auto key = std::type_index(typeid(P));
        if (auto it = registry_.find(key); it != registry_.end()) return static_cast<P*>(it->second);
        auto p   = std::make_unique<P>(*this, std::forward<Args>(args)...);
        auto res = p.get();
        fixed_point_ |= res->fixed_point();
        passes_.emplace_back(std::move(p));
        registry_.emplace(key, res);
        return res;
    }

    /// Runs a single Pass.
    template<class P, class... Args>
    static void run(World& world, Args&&... args) {
        PassMan man(world);
        man.add<P>(std::forward<Args>(args)...);
        man.run();
    }
    ///@}

private:
    /// @name State
    ///@{
    struct State {
        State()                 = default;
        State(const State&)     = delete;
        State(State&&)          = delete;
        State& operator=(State) = delete;
        State(size_t num)
            : data(num) {}

        Def* curr_mut = nullptr;
        DefArray old_ops;
        std::stack<Def*> stack;
        MutMap<undo_t> mut2visit;
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
    Ref rewrite(Ref);

    Ref map(Ref old_def, Ref new_def) {
        curr_state().old2new[old_def] = new_def;
        curr_state().old2new.emplace(new_def, new_def);
        return new_def;
    }

    std::optional<Ref> lookup(Ref old_def) {
        for (auto& state : states_ | std::ranges::views::reverse)
            if (auto i = state.old2new.find(old_def); i != state.old2new.end()) return i->second;
        return {};
    }
    ///@}

    /// @name analyze
    ///@{
    undo_t analyze(Ref);
    bool analyzed(Ref def) {
        for (auto& state : states_ | std::ranges::views::reverse)
            if (state.analyzed.contains(def)) return true;
        curr_state().analyzed.emplace(def);
        return false;
    }
    ///@}

    World& world_;
    std::deque<std::unique_ptr<Pass>> passes_;
    absl::flat_hash_map<std::type_index, Pass*> registry_;
    std::deque<State> states_;
    Def* curr_mut_    = nullptr;
    bool fixed_point_ = false;
    bool proxy_       = false;

    template<class P, class N>
    friend class FPPass;
};

/// Inherit from this class if your Pass does **not** need state and a fixed-point iteration.
template<class P, class N>
class RWPass : public Pass {
public:
    RWPass(PassMan& man, std::string_view name)
        : Pass(man, name) {}

    bool inspect() const override {
        if constexpr (std::is_same<N, Def>::value)
            return man().curr_mut();
        else
            return man().curr_mut()->template isa<N>();
    }

    N* curr_mut() const {
        if constexpr (std::is_same<N, Def>::value)
            return man().curr_mut();
        else
            return man().curr_mut()->template as<N>();
    }
};

/// Inherit from this class using [CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern),
/// if you **do** need a Pass with a state and a fixed-point.
template<class P, class N>
class FPPass : public RWPass<P, N> {
public:
    using Super = RWPass<P, N>;
    using Data  = std::tuple<>; ///< Default.

    FPPass(PassMan& man, std::string_view name)
        : Super(man, name) {}

    bool fixed_point() const override { return true; }

protected:
    /// @name State-related Getters
    ///@{
    const auto& states() const { return Super::man().states_; }
    auto& states() { return Super::man().states_; }
    auto& data() {
        assert(!states().empty());
        return *static_cast<typename P::Data*>(states().back().data[Super::index()]);
    }
    template<size_t I>
    auto& data() {
        return std::get<I>(data());
    }
    /// Use this for your convenience if `P::Data` is a map.
    template<class K>
    auto& data(const K& key) {
        return data()[key];
    }
    /// Use this for your convenience if `P::Data` is a map.
    template<size_t I, class K>
    auto& data(const K& key) {
        return data<I>()[key];
    }
    ///@}

    /// @name undo
    ///@{
    undo_t curr_undo() const { return Super::man().curr_undo(); } ///< Current undo point.

    /// Retrieves the point to backtrack to just **before** @p mut was seen the very first time.
    undo_t undo_visit(Def* mut) const {
        const auto& mut2visit = Super::man().curr_state().mut2visit;
        if (auto i = mut2visit.find(mut); i != mut2visit.end()) return i->second;
        return No_Undo;
    }

    /// Retrieves the point to backtrack to just **before** rewriting @p mut%'s body.
    undo_t undo_enter(Def* mut) const {
        for (auto i = states().size(); i-- != 0;)
            if (states()[i].curr_mut == mut) return i;
        return No_Undo;
    }
    ///@}

private:
    /// @name Memory Management for State
    ///@{
    void* alloc() override { return new typename P::Data(); }
    void* copy(const void* p) override { return new typename P::Data(*static_cast<const typename P::Data*>(p)); }
    void dealloc(void* state) override { delete static_cast<typename P::Data*>(state); }
    ///@}
};

inline World& Pass::world() { return man().world(); }

} // namespace thorin
