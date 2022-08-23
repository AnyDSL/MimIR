#pragma once

#include "thorin/world.h"

#include "dialects/autodiff/autogen.h"

namespace thorin::autodiff {

/*

## Functions

We have multiple functions (/transformations/substitutions)
- tangent_type
- (closed modular) pullback
- partial pullback
- shadow pullback
- derivative

### Tangent Type
Mathematical notation: `⋅ᵀ`
Thorin function name: `tangent_type`
C++ function name: `_________`

Computes the type of a tangent given the type of the input.

// TODO: change mem?, change ptr? change int?
no tangent => zero or dummy?


### Closed Modular Pullback
A special case of the partial pullback for a closed function
resulting in a closed pullback independent of external arguments.
Only depends on the arguments of its own function (and returns their tangent).
Is dependent on the arguments of the function.
Therefore, can only exists in tandem with the function itself in a derivative pair (see below).


### Partial Pullback
Mathematical notation: `f*_f` / `f*_x'`
Thorin function name: `________`
C++ function name: `_________`

Expression: `e: B`
Pullback: `e*: Bᵀ -> Aᵀ` (if the function arguments have type `A`)

Inside a function, we compute the gradients with respect to
the function parameters.
Each (non-closed) expression can be seen as an applied function from function parameters to its value.

// TODO: state and (mentally) valide the relation to the closed pullback


### Shadow Pullback
Mathematical notation: ``
Thorin function name: `________`
C++ function name: `_________`

Also called structure preserving pullback.
An auxiliary C++ collection of pullbacks for container expressions.
For instance, a pointer has a pointer that contains a pullback as shadow pullback.

Expression: `e: C B`
Pullback: `e_S*: C (B*) = e_S*: C (Bᵀ -> Aᵀ)`
for a container `C` (pointer, array, (tuple))

The container acts as functor and accesses to data are liftet to acceses of pullbacks of data.


### Derivative
Mathematical notation: `⋅'`
Thorin function name: `autodiff`
C++ function name: `_________`

Acts on functions and function like objects.
* Functions (closed lambda, higher-order arguments)
* Operations (+, -, ...)
* load, store (TODO: is this possible?)

returns a function that takes the input and returns the output and the closed pullback.
The pullback takes the output tangent and returns the input tangent.

applied to `f: T -> U` returns
`f' = λ x. (f x, f*_x) : T -> U × (U -> T)`

// TODO: unify direct and cps



## Pseudocode

Start with an autodiff call to a function
.let f' = %autodiff.autodiff ... f

Now we create a new function f_diff such that
.let f' = f_diff
.let (y, f*) = f'(x)

How do we convert f to f_diff?
- transform type scheme of f
    - type change enriches function types
    - enrich return continuation
- set partial pullback for function argument to identity
- TODO: (init shadow cells for pointers) TODO: ?(set pointer pb override)
- enrich body with (partial) pullback
    - combine partial pullbacks of subexpressions
    - the partial pullback is the closed pullback for the function
- return both



other viewpoint:
there are no open expressions

*/

// inline is important
// otherwise, the compilation creates two definitions of the functions (irregardless of include guards)
inline const Def* op_autodiff(const Def* fun) {
    World& world = fun->world();
    // we rely on the normalized thorin convention that all arguments in functions are grouped
    // cn[[args], cont?=cn[returns]]
    auto [dom, codom] = fun->type()->as<Pi>()->doms<2>();
    // TODO: do we need mem special casing?
    return world.app(world.app(world.ax<autodiff>(), {dom, codom}), fun);
}

const Def* tangent_type(const Def*);
const Def* augment_type(const Def*);
const Pi* autodiff_type(const Def*);
const Pi* pullback_type(const Def*, const Def*);

} // namespace thorin::autodiff