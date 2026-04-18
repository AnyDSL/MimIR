use egg::RecExpr;
use ffi::bridge::{CostFn, MimNode, RewriteResult, RuleSet};
use mim_egg::Mim;

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

pub fn equality_saturate_slotted(
    sexpr: &str,
    rulesets: Vec<RuleSet>,
    cost_fn: CostFn,
) -> Vec<RewriteResult> {
    mim_slotted::equality_saturate(sexpr, rulesets, cost_fn)
}

pub fn mim_node_str(node: MimNode) -> String {
    format!("{:?}", node)
}

pub fn pretty(sexpr: &str, line_len: usize) -> String {
    let normalized = sexpr.replace("\r\n", "\n");
    let mut sexprs: Vec<&str> = normalized.split("\n\n").collect();
    sexprs.retain(|s| !s.trim().is_empty());
    let mut res = String::new();

    for (i, sexpr) in sexprs.iter().enumerate() {
        let parsed: RecExpr<Mim> = sexpr.parse().unwrap();
        res.push_str(parsed.pretty(line_len).as_str());
        if i < sexprs.len() - 1 {
            res.push_str("\n\n");
        } else {
            res.push('\n');
        }
    }

    res
}
