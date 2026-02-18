use crate::core::*;
use crate::*;

pub mod core;
pub mod math;

// TODO: define the rest of Mim (also add to rexpr_to_vec afterwards)
// and modify accordingly once extern and eval filter added to lambdas in sexpr
define_language! {
    pub enum Mim {
        "lam" = Lam([Id; 2]),
        "con" = Con([Id; 3]),
        "app" = App([Id; 2]),

        "var" = Var([Id; 2]),
        "lit" = Lit(Box<[Id]>),

        "tuple" = Tuple(Box<[Id]>),
        "extract" = Extract([Id; 2]),
        "ins" = Ins([Id; 3]),

        // TYPES
        "sigma" = Sigma(Box<[Id]>),
        "arr" = Arr([Id; 2]),
        "cn" = Cn(Id),
        "idx" = Idx(Id),

        Num(i32), Symbol(String),
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
            type_: _t,
        }) = egraph[id].data.constant.clone()
        {
            let const_id = egraph.add(c);
            let lit_id = egraph.add(Lit(Box::new([const_id])));
            egraph.union(id, lit_id);
        }
    }
}

// Can be used to create conditional rewrite rules like (foo ?a) => (bar ?a) if is_const(var("?a"))
fn _is_const(v: egg::Var) -> impl Fn(&mut EGraph<Mim, MimAnalysis>, Id, &Subst) -> bool {
    move |eg, _, subst| eg[subst[v]].data.constant.is_some()
}

pub fn rules() -> Vec<Rewrite<Mim, MimAnalysis>> {
    let mut rules = Vec::new();

    rules.extend(core::rules());
    rules.extend(math::rules());

    rules
}
