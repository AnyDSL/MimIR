#ifndef THORIN_ERROR_H
#define THORIN_ERROR_H

#include <stdexcept>

#include "thorin/config.h"

namespace thorin {

class Def;

class TypeError : public std::logic_error {
public:
    TypeError(const std::string& what_arg)
        : std::logic_error(what_arg) {}
};

class ErrorHandler {
public:
    virtual ~ErrorHandler() {}

    virtual void expected_shape(const Def* def);
    virtual void index_out_of_range(const Def* arity, const Def* index);
    virtual void ill_typed_app(const Def* callee, const Def* arg);
};

} // namespace thorin

#endif
