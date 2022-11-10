#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/auxiliary/autodiff_flow_analysis.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

class WARAnalysis {
public:

    Lam* lam_;
    explicit WARAnalysis(Lam* lam) : lam_(lam) { run(); }

    void run(){
        scan(lam_);
    }

    const Def* unextract(const Def* mem){
        if(auto extract = mem->isa<Extract>()){
            return extract->tuple();
        }else{
            return mem;
        }
    }

    void propagate(const Def* mem){
        auto op = unextract(mem);

        if( auto app = match<mem::store>(op) ){
            app->dump();
        }else if(auto app = match<mem::load>(op)){
            app->dump();
        }else if(auto app = op->isa<App>()){
            propagate(mem::mem_def(app->arg()));
        }
    }

    void visit(Lam* lam){
        auto body = lam->body()->as<App>();
        auto mem = mem::mem_def(body->arg());

        propagate(mem);

        lam->dump();
    }

    void scan(const Def* def){
        Lam* lam = def->isa_nom<Lam>();
        if(!lam) return;
        auto body = lam->body()->as<App>();

        if( match<affine::For>(body) ){
            scan(body->arg(5));
            scan(body->arg(4));
        }else{
            scan(body->callee());
        }

        visit(lam);
    }

};

} // namespace thorin::autodiff
