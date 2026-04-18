use egg::*;

pub mod analysis;
pub mod rulesets;

define_language! {
    pub enum Mim {
        // TERMS

        // (let <name> <definition> <expression>)
        "let" = Let([Id; 3]),
        // (lam <extern> <name> <domain> <codomain> [<filter>] [<body>])
        "lam" = Lam(Box<[Id]>),
        // (con <extern> <name> <domain> [<filter>] [<body>])
        "con" = Con(Box<[Id]>),
        // (app <callee> <arg>)
        "app" = App([Id; 2]),
        // (var <name> [<proj1> <proj2> ...] <type>)
        "var" = Var(Box<[Id]>),
        // (lit <value> [<type>])
        "lit" = Lit(Box<[Id]>),
        // (pack <arity> <body>)
        "pack" = Pack([Id; 2]),
        // (tuple <value1> <value2> ...)
        "tuple" = Tuple(Box<[Id]>),
        // (extract <tuple> <index>)
        "extract" = Extract([Id; 2]),
        // (ins <tuple> <index> <value>)
        "insert" = Insert([Id; 3]),
        // (rule <name> <meta_var> <lhs> <rhs> <guard>)
        "rule" = Rule([Id; 5]),
        // (inj <type> <value>)
        "inj" = Inj([Id; 2]),
        // (merge <type> <value1> <value2> ...)
        "merge" = Merge(Box<[Id]>),
        // (axm <name> <type>)
        "axm" = Axm([Id; 2]),
        // (match <scrutinee> <arm1> <arm2> ...)
        "match" = Match(Box<[Id]>),
        // (proxy <type> <pass> <tag> <op1> <op2> ...)
        "proxy" = Proxy(Box<[Id]>),


        // TYPES

        // (join <type1> <type2> ...)
        "join" = Join(Box<[Id]>),
        // (meet <type1> <type2> ...)
        "meet" = Meet(Box<[Id]>),
        // (bot <type>)
        "bot" = Bot(Id),
        // (top <type>)
        "top" = Top(Id),
        // (arr <arity> <body>)
        "arr" = Arr([Id; 2]),
        // (sigma <type1> <type2> ...)
        "sigma" = Sigma(Box<[Id]>),
        // (cn <domain>)
        "cn" = Cn(Id),
        // (pi <domain> <codomain>)
        "pi" = Pi([Id; 2]),
        // (idx <size>)
        "idx" = Idx(Id),
        // (hole <type>) - does it even make sense to have this?
        "hole" = Hole(Id),
        // (type <level>)
        "type" = Type(Id),
        // (reform <meta_type>)
        "reform" = Reform(Id),

        Num(i64), Symbol(String),
    }
}
