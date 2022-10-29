#include "dialects/autodiff/passes/autodiff_prep.h"

#include "dialects/core/core.h"
#include "dialects/autodiff/autodiff.h"

namespace thorin::autodiff {


const Def* AutodiffPrep::rewrite(const Def* def) {
    if( auto rop = match<core::rop>(def) ){
        if(rop.id() == core::rop::mul || rop.id() == core::rop::div){
            mark(rop->arg(0));
            mark(rop->arg(1));
        }
    }

    return def;
}

} // namespace thorin::autodiff
