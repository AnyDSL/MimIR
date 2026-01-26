#pragma once

#include <memory>

#include <mim/schedule.h>
#include <mim/phase.h>

namespace mim::plug::buffrize {

namespace detail {

enum class BufferizationChoice {
    Allocate, // Bufferize the def
    Copy,     // Copy the def to a new buffer
    Inplace   // Write into the argument's buffer
};

class AliasSet {
public:
    AliasSet() = default;

    // Find the root of the element with path compression.
    const Def* find(const Def* def);
    // No path compression
    const Def* find(const Def* def) const;

    // Union two elements.
    void unite(const Def* def1, const Def* def2);

    inline const DefSet& get_set(const Def* def) { return sets_[find(def)]; }
    inline const DefSet get_set(const Def* def) const {
        if (auto it = sets_.find(find(def)); it != sets_.end()) return it->second;
        return {};
    }

private:
    void assign_new_parent(const Def* def1, const Def* def2);
    DefSet& get_maybe_new_set(const Def* def);

    absl::flat_hash_map<const Def*, const Def*> parent_;
    DefMap<DefSet> sets_;
};

class InterferenceSchedule {
public:
    explicit InterferenceSchedule(Scheduler& sched);

    enum NodeOrder { BEFORE, AFTER, INDEPENDENT, SAME };
    enum Interference { NONE, YES };

    NodeOrder get_order(const Def* a, const Def* b);
    Interference get_interference(const Def* defA, const Def* defB);

    void dump() const;

private:
    void schedule(Lam* mut, const Def* def);

    Scheduler& sched_;

    DefMap<DefVec> ordered_schedule_;

    DefSet seen_;

    DefMap<DefSet> def_use_map_;
};

class BufferizationAnalysisInfo {
public:
    inline void add_alias(const Def* def1, const Def* def2) {
        def1->world().DLOG("adding aliases {} -- {}", def1, def2);
        ptr_union_find_.unite(def1, def2);
    }
    inline const Def* get_main_pointer(const Def* def) const { return ptr_union_find_.find(def); }
    inline const DefSet get_alias_set(const Def* def) const { return ptr_union_find_.get_set(def); }

    inline void set_mem(const Lam* lam, const Def* mem) { lam2mem_[lam] = mem; }
    inline const Def* get_mem(const Lam* lam) const {
        if (auto it = lam2mem_.find(lam); it != lam2mem_.end()) return it->second;
        return nullptr; // No memory found for this Lam.
    }

    inline void must_copy(const Def* def) { bufferization_plan_.insert({def, BufferizationChoice::Copy}); }
    inline void must_allocate(const Def* def) { bufferization_plan_.insert({def, BufferizationChoice::Allocate}); }
    inline BufferizationChoice get_choice(const Def* def) const {
        if (auto it = bufferization_plan_.find(def); it != bufferization_plan_.end()) return it->second;
        return BufferizationChoice::Inplace;
    }

private:
    // Ptr alias union find
    AliasSet ptr_union_find_;
    // Lam->Mem map
    Def2Def lam2mem_;
    // For each def, we keep track of the bufferization choice.
    DefMap<BufferizationChoice> bufferization_plan_;
};

class BufferizationAnalysis {
public:
    BufferizationAnalysis(const Nest& nest, Scheduler& sched);

    const BufferizationAnalysisInfo& get_analysis_infO() const;

private:
    inline World& world() const { return nest_.world(); }

    // Analyze the nest and fill the analysis information.
    void analyze();
    void analyze(const Def* def);

    void decide(const Def* def);

    const Nest& nest_;
    // Analysis information for bufferization.
    BufferizationAnalysisInfo analysis_info_;
    // List schedule
    InterferenceSchedule interference_;

    DefSet seen_;
};
} // namespace detail

class Bufferize : public NestPhase<Lam> {
public:
    Bufferize(World& world, flags_t annex)
        : NestPhase(world, annex, true) {}

    void visit(const Nest&) override;

private:
    const Def* visit_def(const Def*);

    const Def* visit_insert(Lam* place, const Insert* insert);
    const Def* visit_extract(Lam* place, const Extract* extract);

    const Def* active_mem(Lam* place);
    void add_mem(const Lam* place, const Def* mem);

    Scheduler sched_;
    // Memoization & Association for rewritten defs.
    Def2Def rewritten_;
    // const Def *mem_;
    DefMap<DefVec> lam2mem_;

    // The analysis deciding what to do.
    std::unique_ptr<detail::BufferizationAnalysis> analysis_;
};

} // namespace mim::plug::buffrize
