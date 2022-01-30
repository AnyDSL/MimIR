# Writing Passes

Thorin offers an infrastructure to change a program.
Thorin knows to kind of passes:
1. Rewrite Pass (`RWPass`)
2. Fixed-Point Pass (`FPPass`)

Both passes inherit from `Pass` which provides basic hooks (virtual methods) to do something with your Thorin program.

{:toc}

## Rewrite Pass

### Hooks

#### Inspect

#### Enter

## Fixed-Point Pass

A `FPPass` provides all hooks of a `RWPass`.
In addtion, it also offers the following methods:
