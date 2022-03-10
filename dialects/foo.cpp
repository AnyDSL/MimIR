#include "foo.h"

#include <thorin/config.h>

namespace thorin {

const Def* Foo::rewrite(const Def* def) {
    def->dump();
    return def;
}

} // namespace thorin

extern "C" THORIN_EXPORT thorin::Foo* create(thorin::PassMan& man) { return new thorin::Foo(man); }
extern "C" void THORIN_EXPORT destroy(thorin::Foo* p) { delete p; }
