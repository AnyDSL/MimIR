use crate::mim_std::Mim;
use crate::mim_std::rulesets::core::{CoreData, core_make, core_merge, core_modify};
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
    pub core_data: CoreData,
}

// TODO: Combine results of Analyses for different rulesets (so far only implemented for core)
impl Analysis<Mim> for MimAnalysis {
    type Data = AnalysisData;

    fn merge(&mut self, a: &mut Self::Data, b: Self::Data) -> DidMerge {
        core_merge(a, b)
    }

    fn make(egraph: &mut EGraph<Mim, Self>, enode: &Mim, _id: Id) -> Self::Data {
        core_make(egraph, enode, _id)
    }

    fn modify(egraph: &mut EGraph<Mim, Self>, id: Id) {
        core_modify(egraph, id)
    }
}
