#include <mim/def.h>

#include <mim/ast/parser.h>

namespace mim::ast {
class PyParser {
public:
    PyParser(Parser* parser)
        : parser_(parser) {}

    void plugin(std::string_view plug) {
        auto& ast = parser_->ast();
        auto mod  = parser_->import(plug);
        mod->compile(ast);
    }

private:
    Parser* parser_;
};

} // namespace mim::ast
