- [Link](https://docs.google.com/spreadsheets/d/1oIMw4am_GX5CpX_h4Yvx1G7gr6ZFzOh2OVj2wWkMYw8/edit#gid=0)
- PreOpt: Diffed Thorin Code (directly after ad pass)
  - For Thorin: manual inline annotation switched off
- Opt: Thorin Code right before codegen
- Manual: Mostly modular (without extract) ad approach
- PartialEval: Conceivable partial evaluation (see below)
- FullEval: Fully evaluated ad approach (known result)

## Manual
```
zero* = λ s. (0,0)
pair_sum (da, db) (da', db') = (da + da', db + db')
app' f' (a, a*) = 
  let r, r* = f' a in
  (r, λ s. a* (r* s))
tup' (a, a*) (b, b*) = 
  ((a, b), λ s. pair_sum (a* s) (b* s))
mul' (a,b) = 
  (a*b, λ s. (s * b, s * a))
sub' (a,b) = 
  (a-b, λ s. (s, -s))
const' c = (c, zero*)
pow (a,b) = 
  if b=0 then const' 1 else
    let a' = (a, λ s. (s, 0)) in
    let b' = (b, λ s. (0, s)) in
    let t0' = tup' b' (const' 1) in
    let r1' = app' sub' t0' in
    let t1' = tup' a' r1' in
    let r2' = app' pow t1' in
    let t2' = tup' a' r2' in
    app' mul' t2'
``` 

The code is the augmented code for
```
let r1 = b-1
let r2 = pow (a,r1)
let r3 = a*r2
``` 

## PartialEval
```
pow (a,b) = 
  if b=0 then const' 1 else
    let r,pow* = pow (a,b-1) in
    (
      a*r,
      λ s. 
        let da,db = pow* (s*a) in
        (s*r + da, db)
    )
```

Reasoning:
```
a*  = λ s. (s, 0)
b*  = λ s. (0, s)
t₀* = λ s₀₁. (0, s₀)
r₁* = λ s. t₀* (sub* s) 
    = λ s. (0, s)
t₁* = λ s₀₁. (s₀, s₁) = id
r₂* = λ s. t₁* (pow* s)
    = λ s. pow* s = pow*
t₂* = λ s₀₁. (s₀, 0) + pow* (s₁)
r₃* = λ s. t₂* (mul* s)
    = λ s. t₂* (s * r₂, s * a)
    = λ s. 
        let da,db = pow* (s*a) in
        (s*r₂ + da, db)
``` 


## FullEval
```
pow (a,b) = (a^b, (b*a^(b-1), 0))
```
