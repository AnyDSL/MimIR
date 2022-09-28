#pragma once

#include <deque>
#include <ostream>
#include <string>
#include <string_view>

#include "thorin/util/print.h"
#include "thorin/util/types.h"

#include "absl/container/flat_hash_map.h"

namespace thorin::h {

struct AxiomInfo {
    flags_t tag_id;
    std::string dialect;
    std::string tag;
    std::deque<std::deque<std::string>> subs;
    std::string normalizer;
    bool pi;
};

class Bootstrapper {
public:
    Bootstrapper(std::string_view dialect)
        : dialect_(dialect) {}

    void emit(std::ostream&);
    std::string_view dialect() const { return dialect_; }

    absl::flat_hash_map<std::string, AxiomInfo> axioms;
    Tab tab;

private:
    std::string dialect_;
};

} // namespace thorin::h
