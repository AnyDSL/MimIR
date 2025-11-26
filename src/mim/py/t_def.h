#pragma once
#include <mim/def.h>

namespace mim{
    class MIM_Def{
        private:
            mim::Def* _def;
            
        public:

            MIM_Def(mim::Def* d){
                this->_def = d;
            }

            void make_external(){
                this->_def->make_external();
            }

            void dump(){
                this->_def->dump();
            }

            const mim::Def* def() const {
                return this->_def;
            }
    };
}