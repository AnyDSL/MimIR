#include "dialects/autodiff/analysis/war_analysis.h"

#include "thorin/def.h"
#include "thorin/tuple.h"

#include "dialects/autodiff/analysis/analysis.h"
#include "dialects/autodiff/analysis/analysis_factory.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/builder.h"
#include "dialects/math/math.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

WarAnalysis::WarAnalysis(AnalysisFactory& factory)
    : Analysis(factory)
    , ptr_analysis(factory.ptr()) {
    run();
}

void WarAnalysis::run() {
    /*
    auto& dfa = factory().dfa();

    for( auto node : cfa.post_order() ){
        if(auto lam = node->def()->isa_nom<Lam>()){
            for( auto dfa_node : dfa.post_order(lam) ){
                auto op = dfa_node->def();
                if( match<mem::load>(op) ){
                    op->dump();
                }else if( match<mem::store>(op) ){
                    op->dump();
                }
            }
        }
    }*/

    stores    = &src_stores_;
    auto& cfa = factory().cfa();
    for (auto cfa_node : cfa.post_order()) {
        if (auto lam = cfa_node->def()->isa_nom<Lam>()) { collect(lam); }
    }
    todo_ = true;
    for (; todo_;) {
        todo_ = false;
        for (auto cfa_node : cfa.post_order()) {
            if (auto lam = cfa_node->def()->isa_nom<Lam>()) { wipe(lam); }
        }

        stores = &dst_stores_;
    }

    for (auto cfa_node : cfa.post_order()) {
        if (auto lam = cfa_node->def()->isa_nom<Lam>()) { find(lam); }
    }

    auto size = overwritten.size();

    std::cout << size << std::endl;
    std::cout << size << std::endl;
}

bool WarAnalysis::find(Lam* lam, const Def* mem) {
    auto op = unextract(mem);
    if (auto app = match<mem::store>(op)) {
        auto ptr  = app->arg(1);
        auto node = ptr_analysis.representative(ptr);
        leas.insert(node);
    } else if (auto app = match<mem::load>(op)) {
        auto val  = app->proj(1);
        auto ptr  = app->arg(1);
        auto node = ptr_analysis.representative(ptr);
        if (leas.contains(node)) { overwritten.insert(val); }
    }

    if (auto app = op->isa<App>()) {
        auto mem_arg = mem::mem_def(app->arg());
        find(lam, mem_arg);
    }
}

void WarAnalysis::find(Lam* lam) {
    auto body = lam->body()->as<App>();
    auto mem  = mem::mem_def(body->arg());

    if (mem) {
        leas.clear();
        for (auto store : stores->operator[](lam)) {
            auto ptr = store->as<App>()->arg(1);
            leas.insert(ptr_analysis.representative(ptr));
        }

        find(lam, mem);
    }
}

void WarAnalysis::meet_stores(const Def* src, const Def* dst) {
    if (!src || !dst) return;
    auto& src_stores = stores->operator[](src);
    auto& dst_stores = dst_stores_[dst];
    size_t before    = dst_stores.size();
    dst_stores.insert(src_stores.begin(), src_stores.end());
    size_t after = dst_stores.size();
    if (before != after) { todo_ = true; }
}

void WarAnalysis::wipe(Lam* lam) {
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
        if (auto extract = callee->isa<Extract>()) {
            for (auto branch : extract->tuple()->projs()) { meet_stores(branch, lam); }
        } else {
            meet_stores(callee, lam);
        }
    }
}

bool WarAnalysis::collect(Lam* lam, const Def* mem) {
    auto op = unextract(mem);
    if (auto app = match<mem::store>(op)) { stores->operator[](lam).insert(app); }

    if (auto app = op->isa<App>()) {
        auto mem_arg = mem::mem_def(app->arg());
        collect(lam, mem_arg);
    }
}

void WarAnalysis::collect(Lam* lam) {
    auto body = lam->body()->as<App>();
    auto mem  = mem::mem_def(body->arg());
    if (mem) { collect(lam, mem); }
}

} // namespace thorin::autodiff
