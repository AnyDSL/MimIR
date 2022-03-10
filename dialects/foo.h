#ifndef THORIN_FOO_H
#define THORIN_FOO_H

#include <thorin/pass/pass.h>

namespace thorin {

class Foo : public RWPass<Lam> {
public:
    Foo(PassMan& man)
        : RWPass(man, "foo") {}

    const Def* rewrite(const Def*) override;
};

} // namespace thorin

#endif
