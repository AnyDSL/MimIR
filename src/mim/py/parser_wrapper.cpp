#include <mim/def.h>

#include <mim/ast/parser.h>
namespace mim::ast {
class PyParser {
public:
    PyParser(Parser* parser)
        : parser_(parser) {}

    void plugin(const std::string& plug) {
        auto ast = ast::AST(parser_->driver().world());
        auto mod = parser_->plugin(plug);
        mod->compile(ast);
    }

private:
    Parser* parser_;
};
} // namespace mim::ast
