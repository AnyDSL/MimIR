# Roadmap

## Basic Infrastructure

- [x] arena allocation of nodes
- [x] remove extra data from subclasses of `Def`
- [x] remove RTTI
- [x] `Lam` and `App` instead of `Continuation`
- [x] make analyses work with all mutables, not just `Lam`s
- [x] remove `Type` and make it a `Def`
- [x] PTS calculus of constructions style IR (mostly done)
- [x] remove arg/param lists
- [x] `Sigma`/`Arr` + `Tuple/Pack` and friends
- [x] use `index` and `arity` instead of unsafe `int` for indexing
- [ ] support for multidemnsional indexing (wip)
- [ ] `Union` and friends (wip)
- [ ] `Intersection` and friends (maybe?)
- [x] new recursive `rewrite`
- [ ] remove old `mangle` in favor or `rewrite` (wip)
- [x] remove old `import` in favor of `rewrite`
- [x] remove old `cleanup` in favor of `rewrite`
- [x] make IR extensible

## Memory

- [x] `div`/`mod` with side effect
- [ ] `alloc`/`dealloc` in favor of `slot` and `global`

## Axioms

- [x] use axioms instead of C++ classes
- [x] redesign standard operations to work similar to LLVM
- [ ] remove `Lamm::Intrinsics` in favor of axioms (wip)
- [x] rewrite normalizations
- [x] add function to apply operations polymorphic in rank and dimensions

## Optimizations

- [x] finish new optimizer
    - [x] passes with no fixed point needed
    - [x] passes with fixed point
- [ ] rewrite all optimizations to work with new optimizer
    - [x] inliner
    - [x] partial eval
    - [x] mem2reg
    - [x] scalarize
    - [ ] flatten slots
    - [x] eta red
    - [x] eta exp
    - [x] copy prop     (wip)
    - [x] tail rec elim
    - [ ] closure elim  (wip)
    - [ ] closure conv  (wip)
    - [ ] compile ptrn  (wip)
    - [x] ret\_wrap
    - [ ] reg2mem (for aggregates)
    - [ ] acc prepare (phase that prepares special `vectorize`/`cuda` and friends for code generation)
- [ ] remove old optimizations/passes and `replace` infrastructure (wip)

## Type Checking

- [x] type checking (wip)
- [x] `ErrorHandler` (wip)

## Debugging

- [x] remove static state from logging
- [x] remove `Loc`/`Debug` and make it a `Def`
- [x] add `meta` field
- [x] unit testing with gtest
- [ ] gcov integration

## Module support

- [ ] polish output     (wip)
- [ ] frontend to read it again (wip)
- [ ] integrate with debugging infrastructure

## Backend

- [ ] rewrite C backend
- [x] rewrite LLVM backend

## Future

* Thorin-based vectorizer
* C interface of Thorin?
* Pyhton interface of Thorin?
* add possibility to add new thorin nodes/axioms from frontends

## Far Future

* native backends
