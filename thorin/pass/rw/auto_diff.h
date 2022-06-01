#ifndef THORIN_PASS_RW_AUTO_DIFF_H
#define THORIN_PASS_RW_AUTO_DIFF_H

#include "thorin/pass/pass.h"

namespace thorin {


/// # Automatic Differentiation in Thorin2
///
/// This pass implements automatic differentiation for the following dialects:
/// * core
/// * arithmetic
/// * memory, pointer
/// * tensor
///
/// The automatic differentiation is based on the ideas of Brunel et al, 2020:
/// Backpropagation in the Simply Typed Lambda-Calculus with Linear Negation
///
/// Each expression is augmented with a pullback function that constructs the derivates of the expression.
/// A function f: A -> B is translated by invokation of the rev_diff axiom to a function f': A -> B * (B -> A)
/// the second part of the result is the pullback function that expects the output tangent and returns the input tangent.
///
/// Example call:
/// ```
/// fn f(a: f32) -> f32 {
///     (a * a)
/// }
/// fn main() -> i32 {
///     let Df: fn(f32) -> (f32, fn(f32) -> f32) = rev_diff(f);
///     let Gf: (f32, fn(f32) -> f32) = (Df(4f));
///     let y: f32 = (Gf(0));             // result
///     let pb: fn(f32) -> f32 = (Gf(1)); // pullback
///     let g: f32 = pb(1f);              // gradient
/// }
/// ```
/// f    = λ a: f32. (a * a)
/// f'   = λ a: f32. (f a, f* a s)
/// f* a = λ s: f32. (2 * a * s)
/// In the example above the names are:
/// Df for f', pb for f*
///
///
/// Features:
/// * modular construction
/// * simple transformation
/// * generation of efficient code
/// * pure functions except for memory operations in the orignal code
/// * support for higher order functions
/// * reverse mode
/// * handles control flow (conditions, recursion)
///
/// [old draft of technical details](https://www.overleaf.com/read/gdpfxvzqpfjf)
///
/// difference to the current main branch of AD:
/// for multiple arguments, AD currently expects always flat elements instead of arrays
/// (`[mem, r64, r64, cn[...]]` instead of `cn[mem, <2;r64>, cn [...]]`)
/// the changes in code are:
///
/// ```
///    if (ops[0]->isa<Pi>() && std::all_of(ops.begin() + 1, ops.end(), [&](auto op) { return ops[0] == op; })) return
///    arr(n, ops[0]);
/// ```
/// in `const Def* World::sigma(Defs ops, const Def* dbg)`
///
/// and remove
/// ```
/// if (std::all_of(ops.begin() + 1, ops.end(), [&](auto op) { return ops[0] == op; })) return pack(n, ops[0]);
/// ```
/// in `const Def* World::tuple(const Def* type, Defs ops, const Def* dbg)`
///
/// as well as
/// ```
/// if(auto arr = type->isa<Arr>()){
///    type=arr->body();
/// }else {
///    type=type->op(0);
/// }
/// ```
/// instead of
/// ```
/// type = type->as<Arr>()->body();
/// ```
class AutoDiff : public RWPass<> {
public:
    AutoDiff(PassMan& man)
        : RWPass(man, "auto_diff") {}
    const Def* rewrite(const Def*) override;
};

} // namespace thorin

#endif
