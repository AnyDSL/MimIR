#pragma once

#include <deque>
#include <ostream>
#include <string>
#include <string_view>

#include "thorin/util/print.h"
#include "thorin/util/sym.h"
#include "thorin/util/types.h"

namespace thorin::h {

struct AxiomInfo {
    flags_t tag_id;
    Sym dialect;
    Sym tag;
    std::deque<std::deque<Sym>> subs;
    Sym normalizer;
    bool pi;
};

class Bootstrapper {
public:
    Bootstrapper(Sym dialect)
        : dialect_(dialect) {}

    void emit(std::ostream&);
    Sym dialect() const { return dialect_; }

    SymMap<AxiomInfo> axioms;
    Tab tab;

private:
    Sym dialect_;
};

} // namespace thorin::h
