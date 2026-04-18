use egg::*;
use ffi::bridge::{CostFn, RewriteResult, RuleSet};
use ffi::rec_expr_to_res;
use mim_egg::Mim;
use mim_egg::Mim::*;
use mim_egg::analysis::MimAnalysis;
use mim_egg::rulesets::get_rules;

pub mod ffi;
mod mim_egg;
mod mim_slotted;

// TODO: Implement
pub fn equality_saturate_slotted(
    sexpr: &str,
    rulesets: Vec<RuleSet>,
    cost_fn: CostFn,
) -> Vec<RewriteResult> {
    vec![]
}

pub fn equality_saturate(
    sexpr: &str,
    rulesets: Vec<RuleSet>,
    cost_fn: CostFn,
) -> Vec<RewriteResult> {
    let normalized = sexpr.replace("\r\n", "\n");
    let mut sexprs: Vec<&str> = normalized.split("\n\n").collect();
    sexprs.retain(|s| !s.trim().is_empty());

    let mut rules = get_rules(rulesets);
    convert_rules(&mut sexprs, &mut rules);

    match cost_fn {
        CostFn::AstSize => rewrite_sexprs(sexprs, rules, || AstSize),
        CostFn::AstDepth => rewrite_sexprs(sexprs, rules, || AstDepth),
        _ => panic!("Unknown cost function provided."),
    }
}

fn rewrite_sexprs<C, F>(
    sexprs: Vec<&str>,
    rules: Vec<Rewrite<Mim, MimAnalysis>>,
    cost_fn: F,
) -> Vec<RewriteResult>
where
    C: CostFunction<Mim>,
    F: Fn() -> C,
{
    let mut rewritten_sexprs: Vec<RewriteResult> = Vec::new();

    for sexpr in sexprs {
        let runner = Runner::<Mim, MimAnalysis, ()>::default()
            .with_expr(&sexpr.parse().unwrap())
            .run(&rules);

        let extractor = Extractor::new(&runner.egraph, cost_fn());
        let (_best_cost, best_expr) = extractor.find_best(runner.roots[0]);

        rewritten_sexprs.push(rec_expr_to_res(best_expr));
    }

    rewritten_sexprs
}

fn convert_rules(sexprs: &mut Vec<&str>, rules: &mut Vec<Rewrite<Mim, MimAnalysis>>) {
    // Converts rewrite rules in sexpr form into rewrite rules usable in egg and then
    // filters them out so we only have proper sexprs remaining to equality saturate in the next loop
    sexprs.retain(|sexpr| {
        let parsed: RecExpr<Mim> = sexpr.parse().unwrap();
        if let Some((_id, Mim::Rule([name, meta_var, lhs, rhs, _guard]))) = parsed.items().last() {
            let nth_node = |id: Id| parsed.items().nth(usize::from(id)).unwrap().1.clone();

            let rule_name = if let Symbol(s) = nth_node(*name) {
                s
            } else {
                panic!("Failed to parse rule name.")
            };

            let mut meta_vars: Vec<String> = Vec::new();
            let meta_var_rexpr = nth_node(*meta_var).build_recexpr(nth_node);
            for (_id, node) in meta_var_rexpr.items() {
                if let Mim::Var(ids) = node
                    && let [var_name, ..] = &**ids
                {
                    if let Some((_id, Symbol(s))) =
                        meta_var_rexpr.items().nth(usize::from(*var_name))
                    {
                        meta_vars.push(s.clone());
                    } else {
                        panic!("Failed to parse meta variable name.")
                    };
                }
            }

            let mut lhs_rexpr = nth_node(*lhs).build_recexpr(nth_node);
            for (_id, node) in lhs_rexpr.items_mut() {
                if let Mim::Symbol(s) = node
                    && meta_vars.contains(s)
                {
                    s.insert(0, '?')
                }
            }

            let mut rhs_rexpr = nth_node(*rhs).build_recexpr(nth_node);
            for (_id, node) in rhs_rexpr.items_mut() {
                if let Mim::Symbol(s) = node
                    && meta_vars.contains(s)
                {
                    s.insert(0, '?')
                }
            }

            let pat: Pattern<Mim> = lhs_rexpr.pretty(80).parse().unwrap();
            let outpat: Pattern<Mim> = rhs_rexpr.pretty(80).parse().unwrap();
            let rule: Rewrite<Mim, MimAnalysis> = Rewrite::new(rule_name, pat, outpat).unwrap();
            rules.push(rule);
            false
        } else {
            true
        }
    });
}
