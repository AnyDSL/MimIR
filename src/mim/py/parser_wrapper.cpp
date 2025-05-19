#include<mim/ast/parser.h>
#include<mim/def.h>
namespace mim::ast{
    class PyParser{
        private:
            Parser parser_;
        public:
            void plugin(const std::string& plug){
                parser_.plugin(plug);
                return;
            }


    };
}