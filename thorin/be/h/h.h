#ifndef THORIN_BE_H_H_H
#define THORIN_BE_H_H_H

#include <deque>
#include <ostream>
#include <string>
#include <string_view>

#include "thorin/util/print.h"

namespace thorin::h {

struct AxiomInfo {
    std::string dialect;
    std::string group;
    std::deque<std::deque<std::string>> tags;
    std::string normalizer;
    bool pi;
};

class Bootstrapper {
public:
    Bootstrapper(std::string_view dialect)
        : dialect_(dialect) {}

    void emit(std::ostream&);
    std::string_view dialect() const { return dialect_; }

    std::deque<AxiomInfo> axioms;
    Tab tab;

private:
    std::string dialect_;
};

} // namespace thorin::h

#endif
