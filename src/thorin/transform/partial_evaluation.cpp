#include <iostream>
#include <list>
#include <unordered_map>
#include <queue>

#include "thorin/world.h"
#include "thorin/analyses/scope.h"
#include "thorin/analyses/looptree.h"
#include "thorin/be/thorin.h"
#include "thorin/analyses/top_level_scopes.h"
#include "thorin/transform/mangle.h"
#include "thorin/transform/merge_lambdas.h"

namespace thorin {

static std::vector<Lambda*> top_level_lambdas(World& world) {
    std::vector<Lambda*> result;
    auto scopes = top_level_scopes(world);
    for (auto scope : scopes)
        result.push_back(scope->entry());
    return result;
}

class TraceEntry {
public:
    TraceEntry(Lambda* nlambda, Lambda* olambda) 
        : nlambda_(nlambda)
        , olambda_(olambda)
        , is_evil_(false)
        , todo_(true)
    {}

    bool is_evil() { return is_evil_; }
    bool todo() { return todo_; }
    Lambda* olambda() const { return olambda_; }
    Lambda* nlambda() const { return nlambda_; }
    void dump() const {
        std::cout << olambda()->unique_name() << '/' << nlambda()->unique_name() 
            << " todo: " << todo_ << " evil: " << is_evil_ << std::endl;
    }

private:
    Lambda* nlambda_;
    Lambda* olambda_;
    bool is_evil_;
    bool todo_;
};

class EdgeType {
public:
    EdgeType(bool is_within, int n)
        : is_within_(is_within)
        , n_(n)
    {
        std::cout << (is_within ? "within " : "cross ") << n << std::endl;
    }

    bool is_within() const { return is_within_; }
    bool is_cross() const { return !is_within(); }
    int n() const { return n_; }

private:
    bool is_within_;
    int n_;
};

class PartialEvaluator {
public:
    PartialEvaluator(World& world)
        : world_(world)
        , scope_(world, top_level_lambdas(world))
        , loops_(scope_)
    {
        loops_.dump();
        collect_headers(loops_.root());
        for (auto lambda : world.lambdas())
            new2old_[lambda] = lambda;
    }

    void collect_headers(const LoopNode*);
    void process();
    void rewrite_jump(Lambda* lambda, Lambda* to, ArrayRef<size_t> idxs);
    void remove_runs(Lambda* lambda);
    void update_new2old(const Def2Def& map);
    EdgeType classify(Lambda* src, Lambda* dst) const;
    int order(EdgeType e) const { return e.is_within() ? -2*(e.n()) + 1 : -2*(e.n()); }
    TraceEntry trace_entry(Lambda* lambda) { return TraceEntry(lambda, new2old_[lambda]); }
    void push(Lambda* src, ArrayRef<Lambda*> dst);
    Lambda* pop();
    void dump_edge(Lambda* src, Lambda* dst) {
            std::cout << src->unique_name() << '/' << new2old_[src]->unique_name() << " -> " 
                      << dst->unique_name() << '/' << new2old_[dst]->unique_name() << std::endl;
    }

    const LoopHeader* is_header(Lambda* lambda) const {
        auto i = lambda2header_.find(new2old_[lambda]);
        if (i != lambda2header_.end())
            return i->second;
        return nullptr;
    }

    void dump_trace() {
        for (auto entry : trace_)
            entry.dump();
    }

    World& world_;
    Scope scope_;
    LoopTree loops_;
    Lambda2Lambda new2old_;
    std::unordered_map<Lambda*, const LoopHeader*> lambda2header_;
    std::unordered_set<Lambda*> done_;
    std::list<TraceEntry> trace_;
};

void PartialEvaluator::push(Lambda* src, ArrayRef<Lambda*> dst) {
}

Lambda* PartialEvaluator::pop() {
    return nullptr;
}

EdgeType PartialEvaluator::classify(Lambda* nsrc, Lambda* ndst) const {
    auto src = new2old_[nsrc];
    auto dst = new2old_[ndst];
    auto hsrc = loops_.lambda2header(src);
    auto hdst = loops_.lambda2header(dst);

    std::cout << "classify: " << src->unique_name() << " -> " << dst->unique_name() << std::endl;
    if (is_header(dst)) {
        if (loops_.contains(hsrc, dst))
            return EdgeType(true, hdst->depth() - hsrc->depth()); // within n, n positive
        if (loops_.contains(hdst, src))
            return EdgeType(true, hsrc->depth() - hdst->depth()); // within n, n negative
    }

#ifndef NDEBUG
    for (auto i = hsrc; i != hdst; i = i->parent()) {
        if (i->is_root()) {
            std::cout << "warning: " << std::endl;
            std::cout << src->unique_name() << '/' << nsrc->unique_name() << " -> " << dst->unique_name() << '/' << ndst->unique_name() << std::endl;
            return EdgeType(false, hdst->depth() - hsrc->depth());
        }
        assert(!i->is_root());
    }
#endif
    return EdgeType(false, hdst->depth() - hsrc->depth());// cross n, n <= 0
}

void PartialEvaluator::collect_headers(const LoopNode* n) {
    if (const LoopHeader* header = n->isa<LoopHeader>()) {
        for (auto lambda : header->lambdas())
            lambda2header_[lambda] = header;
        for (auto child : header->children())
            collect_headers(child);
    }
}

void PartialEvaluator::process() {
    for (auto src : top_level_lambdas(world_)) {
        trace_.clear();

        while ((src = pop())) {
            std::cout << "src: " << src->unique_name() << std::endl;
            std::cout << "loop stack:" << std::endl;
            std::cout << "----" << std::endl;
            //for (auto& info : loop_stack_)
                //std::cout << info.loop()->lambdas().front()->unique_name() << std::endl;
            std::cout << "----" << std::endl;

            emit_thorin(world_);
            assert(!src->empty());

            auto succs = src->direct_succs();
            bool fold = false;

            auto to = src->to();
            if (auto run = to->isa<Run>()) {
                to = run->def();
                fold = true;
            }

            Lambda* dst = to->isa_lambda();

            if (dst == nullptr) {
                push(src, succs);
                dump_trace();
                continue;
            }

            std::vector<Def> f_args, r_args;
            std::vector<size_t> f_idxs, r_idxs;

            for (size_t i = 0; i != src->num_args(); ++i) {
                if (auto evalop = src->arg(i)->isa<EvalOp>()) {
                    if (evalop->isa<Run>()) {
                        f_args.push_back(evalop);
                        r_args.push_back(evalop);
                        f_idxs.push_back(i);
                        r_idxs.push_back(i);
                        fold = true;
                    } else
                        assert(evalop->isa<Halt>());
                } else {
                    f_args.push_back(src->arg(i));
                    f_idxs.push_back(i);
                }
            }

            if (!fold) {
                push(src, {dst});
                continue;
            } else {
                if (is_header(new2old_[dst])) {
                    //auto e = classify(src, dst);
                    //if (e.is_within() && e.n() <= 0) {
                        //if (loop_stack_.back().is_evil())
                            //continue;
                    //}
                }
            }

            Scope scope(dst);
            Def2Def f_map;
            GenericMap generic_map;
            bool res = dst->type()->infer_with(generic_map, src->arg_pi());
            assert(res);
            auto f_to = drop(scope, f_map, f_idxs, f_args, generic_map);
            f_map[to] = f_to;
            update_new2old(f_map);

            if (f_to->to()->isa_lambda() 
                    || (f_to->to()->isa<Run>() && f_to->to()->as<Run>()->def()->isa_lambda())) {
                rewrite_jump(src, f_to, f_idxs);
                for (auto lambda : scope.rpo()) {
                    auto mapped = f_map[lambda]->as_lambda();
                    if (mapped != lambda)
                        mapped->update_to(world_.run(mapped->to()));
                }
                push(src, {f_to});
            } else {
                Def2Def r_map;
                auto r_to = drop(scope, r_map, r_idxs, r_args, generic_map);
                r_map[to] = r_to;
                update_new2old(r_map);
                rewrite_jump(src, r_to, r_idxs);
                push(src, {r_to});
            }
        }
    }
}

void PartialEvaluator::rewrite_jump(Lambda* lambda, Lambda* to, ArrayRef<size_t> idxs) {
    std::vector<Def> new_args;
    size_t x = 0;
    for (size_t i = 0, e = lambda->num_args(); i != e; ++i) {
        if (x < idxs.size() && i == idxs[x])
            ++x;
        else
            new_args.push_back(lambda->arg(i));
    }

    lambda->jump(to, new_args);
}

void PartialEvaluator::remove_runs(Lambda* lambda) {
    for (size_t i = 0, e = lambda->size(); i != e; ++i) {
        if (auto run = lambda->op(i)->isa<Run>())
            lambda->update_op(i, run->def());
    }
}

void PartialEvaluator::update_new2old(const Def2Def& old2new) {
    for (auto p : old2new) {
        if (auto olambda = p.first->isa_lambda()) {
            auto nlambda = p.second->as_lambda();
            //std::cout << nlambda->unique_name() << " -> "  << olambda->unique_name() << std::endl;
            assert(new2old_.contains(olambda));
            new2old_[nlambda] = new2old_[olambda];
        }
    }
}

//------------------------------------------------------------------------------

void partial_evaluation(World& world) { 
    emit_thorin(world);
    PartialEvaluator(world).process(); 
}

}
