This document contains the current issues of the test suite.

## AutoDiff
### `2out.thorin`
Old variables are accessed.
This error is most likely caused by DS2CPS and should be resolved there.

### `pow_autodiff.thorin`
Eta conversion generates wrapper functions that are manipulated by CopyProp.
In the end, a function is created that contains an open function.
But the open function looks like a closed function.

The solution is to handle the open functions similar to continuations.

## General

### `simple_real.thorin`

The `real` type needs a dialect.


## Second Order

The general error seems to be that some axiom calls (like cps2ds) are left and are not correctly translated.

The solution is to fully simplify the program before performing a second translation.
This procedure needs the more sophisticated optimization pipeline.

### `double_diff.thorin`
Error at call of function.

### `.thorin`
Pi types as arguments to HO axioms (CPS2DS) are not translated.

