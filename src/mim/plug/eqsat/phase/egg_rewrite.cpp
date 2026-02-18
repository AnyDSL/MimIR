#include "mim/plug/eqsat/phase/egg_rewrite.h"

#include "mim/plug/core/be/sexpr.h"

// TODO: we have to somehow include the build
// of this header file in cmake as it is now manually
// built from ../egg/ using 'cargo build --release'
// Also, we might want the built files to end up
// in the build directory rather than here in the src directory
#include "../egg/target/cxxbridge/eqsat/src/lib.rs.h"

// TODO: implement EggRewrite, a Phase that will
// transform the given world into an optimized world in three steps:
//
// 1) core::be::sexpr::emit is utilized to print a symbolic expression
//    representing the initial world into a string byte stream. (this plugin will
//    have to make the core plugin into a dependency for this)
// 2) egg::src::lib::equality_saturate(sexpr: &str)->Vec<MimNode> is called and returns an optimized
//    representation of the new world in the form of a recursive expression.
//    (we probably want to have some mechanism of passing mim-defined rules to
//    this function as well later on)
// 3) the recursive expression gets decoded and transformed into the new world
//    which shall be the end result of running the EggRewrite phase

namespace mim::plug::eqsat {

/*
 * Egg Rewrite
 */
void EggRewrite::start() {
    std::ostringstream sexpr;
    sexpr::emit(old_world(), sexpr);

    auto rec_expr = equality_saturate(sexpr.str());

    // TODO: should define an enum for
    // mim node variants at the FFI so node.variant is typed as that enum
    for (auto node : rec_expr)
        node.variant = node.variant;
}

} // namespace mim::plug::eqsat
