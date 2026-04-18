use slotted_egraphs::*;

define_language! {
    pub enum MimSlotted {
        // TERMS

        // (let <name> <definition> <expression>)
        Let(Bind<AppliedId>, AppliedId, AppliedId) = "let",
        // (lam <extern> <name> <var-name> <domain-type> <codomain-type> <filter> <body>)
        Lam(AppliedId, AppliedId, Bind<AppliedId>, AppliedId, AppliedId, AppliedId, AppliedId) = "lam",
        // (con <extern> <name> <var-name> <domain-type> <filter> <body>)
        Con(AppliedId, AppliedId, Bind<AppliedId>, AppliedId, AppliedId, AppliedId) = "con",
        // (app <callee> <arg>)
        App(AppliedId, AppliedId) = "app",
        // (var <name>)
        Var(Slot) = "var",
        // (lit <value> <type>)
        Lit(AppliedId, AppliedId) = "lit",
        // (pack <arity> <body>)
        Pack(AppliedId, AppliedId) = "pack",
        // (tuple <elem-cons>)
        Tuple(AppliedId) = "tuple",
        // (extract <tuple> <index>)
        Extract(AppliedId, AppliedId) = "extract",
        // (insert <tuple> <index> <value>)
        Insert(AppliedId, AppliedId, AppliedId) = "insert",
        // (rule <name> <meta_var> <lhs> <rhs> <guard>)
        Rule(Symbol, AppliedId, AppliedId, AppliedId, AppliedId) = "rule",
        // (inj <type> <value>)
        Inj(AppliedId, AppliedId) = "inj",
        // (merge <type> <type-cons>)
        Merge(AppliedId, AppliedId) = "merge",
        // (axm <name> <type>)
        Axm(AppliedId, AppliedId) = "axm",
        // (match <op-cons>)
        Match(AppliedId) = "match",
        // (proxy <type> <pass> <tag> <op-cons>)
        Proxy(AppliedId, AppliedId, AppliedId, AppliedId) = "proxy",


        // TYPES

        // (join <type-cons>)
        Join(AppliedId) = "join",
        // (meet <type-cons>)
        Meet(AppliedId) = "meet",
        // (bot <type>)
        Bot(AppliedId) = "bot",
        // (top <type>)
        Top(AppliedId) = "top",
        // (arr <arity> <body>)
        Arr(AppliedId, AppliedId) = "arr",
        // (sigma <type-cons>)
        Sigma(AppliedId) = "sigma",
        // (cn <domain>)
        Cn(AppliedId) = "cn",
        // (pi <domain> <codomain>)
        Pi(AppliedId, AppliedId) = "pi",
        // (idx <size>)
        Idx(AppliedId) = "idx",
        // (hole <type>) - does it even make sense to have this?
        Hole(AppliedId) = "hole",
        // (type <level>)
        Type(AppliedId) = "type",
        // (reform <meta_type>)
        Reform(AppliedId) = "reform",

        // Enables variadic language constructs such as Tuple, Sigma, Match, ...
        Cons(AppliedId, AppliedId) = "cons",
        Nil() = "nil",

        // Leaf nodes
        Num(i64),
        Symbol(Symbol),
    }
}
