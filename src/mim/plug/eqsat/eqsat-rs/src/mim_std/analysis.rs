use crate::mim_std::Mim;
use crate::mim_std::Mim::Lit;
use crate::mim_std::rulesets::core::fold_core;
use egg::*;

#[macro_export]
macro_rules! find_node {
    ($egraph:expr, $id:expr, $pat:pat => $val:expr) => {
        $egraph[*$id]
            .nodes
            .iter()
            .find_map(|node| if let $pat = node { Some($val) } else { None })
    };
}

#[derive(Default, Clone)]
pub struct MimAnalysis;
#[derive(Debug)]
pub struct AnalysisData {
    pub constant: Option<Const>,
}

// TODO: this is specific to the core ruleset so it shouldn't be in this file
#[derive(Debug, Clone)]
pub struct Const {
    pub val: Option<Mim>,
    pub type_: Option<Mim>,
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
