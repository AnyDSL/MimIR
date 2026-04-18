use ffi::bridge::{CostFn, MimNode, RewriteResult, RuleSet};

pub mod ffi;
mod mim_egg;
mod mim_slotted;

pub fn equality_saturate(
    sexpr: &str,
    rulesets: Vec<RuleSet>,
    cost_fn: CostFn,
) -> Vec<RewriteResult> {
    mim_egg::equality_saturate(sexpr, rulesets, cost_fn)
}

pub fn pretty(sexpr: &str, line_len: usize) -> String {
    mim_egg::pretty(sexpr, line_len)
}

pub fn equality_saturate_slotted(
    sexpr: &str,
    rulesets: Vec<RuleSet>,
    cost_fn: CostFn,
) -> Vec<RewriteResult> {
    mim_slotted::equality_saturate(sexpr, rulesets, cost_fn)
}

pub fn pretty_slotted(sexpr: &str, line_len: usize) -> String {
    mim_slotted::pretty(sexpr, line_len)
}

pub fn mim_node_str(node: MimNode) -> String {
    format!("{:?}", node)
}
