#pragma once

#include <mim/lam.h>
#include "t_def.h"
#include "t_def.h"
namespace mim{

    class MIM_Lam : public MIM_Def{

        public:
            MIM_Lam(mim::Lam* lam): MIM_Def(lam){

            }



            MIM_Lam* app(MIM_Def* callee, MIM_Def* arg){
                MIM_Lam* p = new MIM_Lam(this->_lam->app(true, callee->into_inner(), arg->into_inner()));
                return p;
            }
            const MIM_Def* var(){
                const MIM_Def* p = new MIM_Def(const_cast<mim::Def*>(this->_lam->var()));
                return p;
            }

            const Lam* lam() const {
                return static_cast<Lam*>(def());
            }

    };
}