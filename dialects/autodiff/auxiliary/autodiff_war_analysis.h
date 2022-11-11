#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/auxiliary/autodiff_lea_analysis.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

class WARAnalysis {
public:
    Lam* lam_;
    LEAAnalysis lea_analysis;
    DefSet found_;
    DefSet overwritten;

    std::vector<Lam*> lams;

    DefMap<DefSet>& stores;
    DefMap<DefSet> src_stores_;
    DefMap<DefSet> dst_stores_;
    DefSet leas;
    bool todo_ = false;

    explicit WARAnalysis(Lam* lam)
        : lam_(lam)
        , lea_analysis(lam)
        , stores(src_stores_) {
        run();
    }

    bool is_overwritten(const Def* def) { return overwritten.contains(def); }

    void run() {
        build(lam_);
        for (Lam* lam : lams) { collect(lam); }
        todo_ = true;
        for (; todo_;) {
            todo_ = false;
            for (Lam* lam : lams) { wipe(lam); }
            stores = dst_stores_;
        }
        // for (Lam* lam : lams) { collect(lam, false); }
        for (Lam* lam : lams) { find(lam); }
    }

    const Def* unextract(const Def* mem) {
        if (auto extract = mem->isa<Extract>()) {
            return unextract(extract->tuple());
        } else {
            return mem;
        }
    }

    bool find(Lam* lam, const Def* mem) {
        auto op = unextract(mem);
        if (auto app = match<mem::store>(op)) {
            auto ptr  = app->arg(1);
            auto node = lea_analysis.representative(ptr);
            leas.insert(node);
        } else if (auto app = match<mem::load>(op)) {
            auto val  = app->proj(1);
            auto ptr  = app->arg(1);
            auto node = lea_analysis.representative(ptr);
            if (leas.contains(node)) { overwritten.insert(val); }
        }

        if (auto app = op->isa<App>()) {
            auto mem_arg = mem::mem_def(app->arg());
            find(lam, mem_arg);
        }
    }

    void find(Lam* lam) {
        auto body = lam->body()->as<App>();
        auto mem  = mem::mem_def(body->arg());

        leas.clear();
        for (auto store : stores[lam]) {
            auto ptr = store->as<App>()->arg(1);
            leas.insert(lea_analysis.representative(ptr));
        }

        find(lam, mem);
    }

    void meet_stores(const Def* src, const Def* dst) {
        if (!src || !dst) return;
        auto& src_stores = stores[src];
        auto& dst_stores = dst_stores_[dst];
        size_t before    = dst_stores.size();
        dst_stores.insert(src_stores.begin(), src_stores.end());
        size_t after = dst_stores.size();
        if (before != after) { todo_ = true; }
    }

    void wipe(Lam* lam) {
        auto body = lam->body()->as<App>();
        auto mem  = mem::mem_def(body->arg());

        if (match<affine::For>(body)) {
            auto loop = body->arg(4)->as_nom<Lam>();
            auto exit = body->arg(5);

            meet_stores(exit, lam);
            meet_stores(loop, lam);
            meet_stores(exit, loop->ret_var());
            meet_stores(loop, loop->ret_var());
        } else {
            auto callee = body->callee();
            if(auto extract = callee->isa<Extract>()){
                for(auto branch : extract->tuple()->projs()){
                    meet_stores(branch, lam);
                }
            }else{
                meet_stores(callee, lam);
            }
        }
    }

    bool collect(Lam* lam, const Def* mem) {
        auto op = unextract(mem);
        if (auto app = match<mem::store>(op)) { stores[lam].insert(app); }

        if (auto app = op->isa<App>()) {
            auto mem_arg = mem::mem_def(app->arg());
            collect(lam, mem_arg);
        }
    }

    void collect(Lam* lam) {
        auto body = lam->body()->as<App>();
        auto mem  = mem::mem_def(body->arg());
        collect(lam, mem);
    }

    void build(const Def* def) {
        Lam* lam = def->isa_nom<Lam>();
        if (!lam) { return; }
        auto body = lam->body();
        
        if(auto app = body->as<App>()){
            auto callee = app->callee();
            if (match<affine::For>(app)) {
                build(app->arg(5));
                build(app->arg(4));
            } else if(auto extract = callee->isa<Extract>()){
                for(auto branch : extract->tuple()->projs()){
                    build(branch);
                }
            } else {
                build(callee);
            }
        }

        lams.push_back(lam);
    }
};

} // namespace thorin::autodiff
