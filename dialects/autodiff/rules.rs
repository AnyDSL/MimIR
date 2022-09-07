// easy readable:

/*

we define two substitutions / translations:
(and an additional one for helper functions)

closed functions:
D : (A -> B) -> (A' -> B' × (Bᵗ -> Aᵗ))
such that
f' = D f x = (f x, f*ₓ)
especially 
#1 (D f x)   = f x
#2 (D f x) s = f*ₓ s = s ⋅ ∇ₓf

open expressions (expressions in a function as implicit applied function in its arguments):
in a function f: A -> ...
D_A : E -> (E' × (Eᵗ -> Aᵗ))
D_A e = (e⁺, e*)
e⁺ = e

e_λ = λ a. e
e   = e_λ a

#1 (D_A e) = e
#2 (D_A e) = #2 (D e_λ a)

type rules:
(A -> B)' = A' -> B' × (Bᵗ -> Aᵗ)
Aᵗ = A



Now on to the rules:

Literal c:E
D_A c = (c, λ s. (0:A))

Application
D_A (f e) =
    let f'        = D f in
    let (e, e*)   = D_A e in
    let (y, f*_y) = f' e in
    (y, e* ∘ f*_y)

Note: we use the more modular appraoch to
just apply f' with e and separate the pullbacks
Types:
f         : E -> Y
f'        : E' -> Y' × (Yᵗ -> Eᵗ)
e         : E
e*        : Eᵗ -> Aᵗ
e* ∘ f*_y : Yᵗ -> Aᵗ

D_A (e1, ..., e_n) =
    let (e_i') = D_A ei in
    let (e_i, e_i*) = e_i' in
    ((e_1, ..., e_n), 
     λ (s_1, ..., s_n). Σ_A (e_i* s_i))


For closed functions:
D mul = 
    λ (a,b).
        (mul (a,b),
         λ s. (mul (s, b), mul (s, a)))

D load =


*/


/// detailed rules (now in cps)
/// we omit beta force at autodiff_type and tangent_type (will be normalized or added in type check)

/// types

.rule (T:*) :
    (%autodiff.tangent_type T) ->
    T;

.rule (A B:*) :
    (%autodiff.autodiff_type (.Cn [A, .Cn B])) ->
    (
        .Cn [
            %autodiff.autodiff_type A,
            .Cn[
                %autodiff.autodiff_type B,
                .Cn[
                    %autodiff.tangent_type B,
                    .Cn [%autodiff.tangent_type A]
                ]
            ]
        ]
    );

/// otherwise
/// pseudo rule
.rule (A E:*) :
    (%autodiff.inner_autodiff_type A (.Cn E)) ->
    (.Cn [
        %autodiff.autodiff_type E, 
        .Cn[
            %autodiff.tangent_type E,
            .Cn [%autodiff.tangent_type A]
        ]
    ]);

/// otherwise
.rule (A:*) :
    (%autodiff.autodiff_type A) ->
    A;








/// inner_autodiff

/// literal
// first step could be inlined
.cn zero_pb [[A:*, E:*], 
    ret:.Cn[
        %autodiff.tangent_type E, 
        .Cn (%autodiff.tangent_type A)
    ]] = {
    .cn inner_pb [
        s  : %autodiff.tangent_type E, 
        ret: .Cn (%autodiff.tangent_type A)
    ] = {
        let z = %autodiff.zero (%autodiff.tangent_type A) in
        ret z
    };
    ret inner_pb
};

.rule (A:*) (n:.Nat) (c:(%Int n)) :
    (%autodiff.inner_autodiff A c) ->
    (c, %direct.cps2ds_dep (...) zero_pb (A, %Int n));

/// application

.cn compose [[A:*, B:*, C:*],
    ret: .Cn[
        [
            .Cn [B, .Cn C],
            .Cn [A, .Cn B]
        ],
        .Cn [.Cn[A, .Cn C]]
    ]] = {
    .cn composition [
        [
            f: .Cn [B, .Cn C],
            g: .Cn [A, .Cn B]
        ],
        ret: .Cn [.Cn[A, .Cn C]]
    ] = {
        .cn res [
            a: A,
            ret: .Cn C
        ] = {
            .cn cont [b: B] = {
                f (b, ret)
            };
            g (a, cont)
        };
        ret res
    };
    ret composition
}

// the application needs to be inspected on meta-level

// nested higher-order application
// cps (Y=⊥) and non-cps
.rule (A E X Y:*) (g:E->X->Y) (e:E):
    (%autodiff.inner_autodiff A (g e)) ->
    (
        .let g_diff        = %autodiff.autodiff g;
        .let (e_aug, e_pb) = %autodiff.inner_autodiff A e;
        g_diff e_aug
    );


/// operator application (axioms) - direct style
.rule (A E Y:*) (g:E->Y) (e:E):
    (%autodiff.inner_autodiff A (g e)) ->
    .let g_diff        = %autodiff.autodiff g;
    .let (e_aug, e_pb) = %autodiff.inner_autodiff A e;
    .let (y, g_pb)     = g_diff e_aug;
    (
        y,
        %direct.cps2ds_dep ... (%direct.cps2ds_dep ... compose ...) (e_pb,g_pb)
    );

/// closed function application
.rule (A E Y:*) (g:.Cn[E,.Cn Y]) (args:[E,.Cn Y]):
    (%autodiff.inner_autodiff A (g args)) ->
    .let g_diff            = %autodiff.autodiff g;
    .let aug_args,         = %autodiff.inner_autodiff A args;
    .let (aug_e, aug_cont) = aug_args;
    .let e_pb              = ... // from augmentation
    .cn g_ret_cont [
        y: %autodiff.autodiff_type Y,
        g_pb: .Cn [
            %autodiff.tangent_type Y,
            .Cn [%autodiff.tangent_type E]
        ]
    ] = {
        aug_cont (
            y, 
            %direct.cps2ds_dep ... (%direct.cps2ds_dep ... compose ...) (e_pb,g_pb)
        )
    };
    (
        g_diff (aug_e, g_ret_cont),
        _
    );

/// open function application (continuation - if, ret, ...)
.rule (A E:*) (f:.Cn E) (e:E):
    (%autodiff.inner_autodiff A (f e)) ->
    .let f_diff        = %autodiff.autodiff f;
    .let (e_aug, e_pb) = %autodiff.inner_autodiff A e;
    (f_diff (e_aug, e_pb), _);
    ;


/// tuple
// TODO: correct syntax?
.rule (A:*) (n:.Nat) (ET:«n; *») (t:«i:n; ET#i»):
    (%autodiff.inner_autodiff A t) ->
    .let t_diff = ‹i:n; %autodiff.inner_autodiff A (t#i) ›;
    .let t_aug = ‹i:n; (t_diff#i)#(0:(%Int 2)) ›;
    .cn t_pb [
        s: «i:n; %autodiff.tangent_type (ET#i) »,
        ret: .Cn (%autodiff.tangent_type A)
    ] = {
        ret (%autodiff.sum (n, %autodiff.tangent_type A) (
            ‹i:n; %direct.cps2ds_dep ... ((t_diff#i)#(1:(%Int 2))) (s#i) ›
        ))
    };
    (
        t_aug,
        t_pb
    );

/// tuple shadow
.rule (A:*) (n:.Nat) (ET:«n; *») (t:«i:n; ET#i»):
    (%autodiff.inner_shadow_autodiff A t) ->
    .let t_diff = ‹i:n; %autodiff.inner_autodiff A (t#i) ›;
    ‹i:n; (t_diff#i)#(1:(%Int 2)) ›;


/// projection
// needs meta reasoning (like app)
.rule (A:*) (n:.Nat) (ET:«n; *») (t:«i:n; ET#i») (j:(%Int n)):
    (%autodiff.inner_autodiff A (t#j)) ->
    .let (t_aug, t_pb) = %autodiff.inner_autodiff A t;
    .let (j_aug, _) = %autodiff.inner_autodiff A j;
    .let t_pb_S = %autodiff.inner_shadow_autodiff A t;
    (
        t_aug,
        t_pb_S#j_aug
    );

// if no shadow pb
.rule (A:*) (n:.Nat) (ET:«n; *») (t:«i:n; ET#i») (j:(%Int n)):
    (%autodiff.inner_autodiff A (t#j)) ->
    .let (t_aug, t_pb) = %autodiff.inner_autodiff A t;
    .let (j_aug, _) = %autodiff.inner_autodiff A j;
    .cn e_pb [
        s: %autodiff.tangent_type (ET#j),
        ret: .Cn (%autodiff.tangent_type A)
    ] = {
        .let s_t = insert (
                %autodiff.zero («i:n; %autodiff.tangent_type (ET#i)»),
                j_aug,
                s
            );
        t_pb (s_t,ret)
    };
    (
        t_aug,
        e_pb
    );



/// glue code: 
/// - var association
/// - open continuation association (pb args)








/// autodiff (closed axioms)


.rule:
    (%autodiff.autodiff A (%)) ->


/// glue code: 
/// - function creation, association
/// - variable association