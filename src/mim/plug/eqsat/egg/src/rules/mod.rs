use crate::core::*;
use crate::*;

pub mod core;
pub mod math;

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

        // (var <name> <type>)
        "var" = Var([Id; 2]),
        // (lit <value> [<type>])
        "lit" = Lit(Box<[Id]>),

        // (pack <arity> <body>)
        "pack" = Pack([Id; 2]),
        // (tuple <value1> <value2> ...)
        "tuple" = Tuple(Box<[Id]>),
        // (extract <tuple> <index>)
        "extract" = Extract([Id; 2]),
        // (ins <tuple> <index> <value>)
        "ins" = Ins([Id; 3]),

        // (rule <lhs> <rhs> <guard>)
        "rule" = Rule([Id; 3]),


        // TYPES

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

#[macro_export]
macro_rules! find_node {
    ($egraph:expr, $id:expr, $pat:pat => $val:expr) => {
        $egraph[*$id]
            .nodes
            .iter()
            .find_map(|node| if let $pat = node { Some($val) } else { None })
    };
}

#[derive(Default)]
pub struct MimAnalysis;
#[derive(Debug)]
pub struct AnalysisData {
    constant: Option<Const>,
}

#[derive(Debug, Clone)]
pub struct Const {
    val: Option<Mim>,
    type_: Option<Mim>,
}

// TODO: ruleset-specific analyses
impl Analysis<Mim> for MimAnalysis {
    type Data = AnalysisData;

    fn merge(&mut self, a: &mut Self::Data, b: Self::Data) -> DidMerge {
        if a.constant.is_none() && b.constant.is_some() {
            a.constant = b.constant;
            DidMerge(true, false)
        } else {
            DidMerge(false, false)
        }
    }

    fn make(egraph: &mut EGraph<Mim, Self>, enode: &Mim, _id: Id) -> Self::Data {
        AnalysisData {
            constant: fold_core(egraph, enode),
        }
    }

    fn modify(egraph: &mut EGraph<Mim, Self>, id: Id) {
        if let Some(Const {
            val: Some(c),
            type_: t_,
        }) = egraph[id].data.constant.clone()
        {
            let const_id = egraph.add(c);
            if let Some(t) = t_ {
                // Case 1: (lit <const> <type>)
                let type_id = egraph.add(t);
                let lit_id = egraph.add(Lit(Box::new([const_id, type_id])));
                egraph.union(id, lit_id);
            } else {
                // Case 2: (lit <const>)
                let lit_id = egraph.add(Lit(Box::new([const_id])));
                egraph.union(id, lit_id);
            }
        }
    }
}

// Can be used to create conditional rewrite rules like (foo ?a) => (bar ?a) if is_const(var("?a"))
fn _is_const(v: egg::Var) -> impl Fn(&mut EGraph<Mim, MimAnalysis>, Id, &Subst) -> bool {
    move |eg, _, subst| eg[subst[v]].data.constant.is_some()
}

pub fn rules(rulesets: Vec<RuleSet>) -> Vec<Rewrite<Mim, MimAnalysis>> {
    let mut rules = Vec::new();

    for ruleset in rulesets {
        match ruleset {
            RuleSet::Core => rules.extend(core::rules()),
            RuleSet::Math => rules.extend(math::rules()),
            _ => (),
        }
    }

    rules
}
