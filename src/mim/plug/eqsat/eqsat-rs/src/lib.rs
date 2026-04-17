use crate::mim_std::Mim;
use crate::mim_std::Mim::*;
use crate::mim_std::analysis::MimAnalysis;
use crate::mim_std::rulesets::get_rules;
use egg::*;
use ffi::{CostFn, MimKind, MimNode, RewriteResult, RuleSet};

mod mim_std;

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

        rewritten_sexprs.push(rexpr_to_res(best_expr));
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

#[cxx::bridge]
pub mod ffi {
    #[derive(Debug)]
    enum RuleSet {
        Core,
        Math,
    }

    #[derive(Debug)]
    enum CostFn {
        AstSize,
        AstDepth,
    }

    #[derive(Debug)]
    enum MimKind {
        Let,
        Lam,
        Con,
        App,
        Var,
        Lit,
        Pack,
        Tuple,
        Extract,
        Insert,
        Rule,
        Inj,
        Merge,
        Axm,
        Match,
        Proxy,
        Join,
        Meet,
        Bot,
        Top,
        Arr,
        Sigma,
        Cn,
        Pi,
        Idx,
        Hole,
        Type,
        Num,
        Symbol,
        Reform,
    }

    #[derive(Debug)]
    struct MimNode {
        kind: MimKind,
        children: Vec<u32>,
        num: i64,
        symbol: String,
    }

    #[derive(Debug)]
    struct RewriteResult {
        value: Vec<MimNode>,
    }

    extern "Rust" {
        fn equality_saturate(
            sexpr: &str,
            rulesets: Vec<RuleSet>,
            cost_fn: CostFn,
        ) -> Vec<RewriteResult>;
        fn mim_node_str(node: MimNode) -> String;
        fn pretty(sexpr: &str, line_len: usize) -> String;
    }
}

fn new_mim(kind: MimKind, children: &[Id], num: i64, symbol: String) -> MimNode {
    let mut converted_ids = Vec::new();
    for id in children {
        converted_ids.push(usize::from(*id) as u32);
    }

    MimNode {
        kind,
        children: converted_ids,
        num,
        symbol,
    }
}

fn rexpr_to_res(rexpr: RecExpr<Mim>) -> RewriteResult {
    let mut nodes = Vec::new();

    for node in rexpr.as_ref() {
        match node {
            Let(children) => nodes.push(new_mim(MimKind::Let, children, 0, String::new())),
            Lam(children) => nodes.push(new_mim(MimKind::Lam, children, 0, String::new())),
            Con(children) => nodes.push(new_mim(MimKind::Con, children, 0, String::new())),
            App(children) => nodes.push(new_mim(MimKind::App, children, 0, String::new())),
            Mim::Var(children) => nodes.push(new_mim(MimKind::Var, children, 0, String::new())),
            Lit(children) => nodes.push(new_mim(MimKind::Lit, children, 0, String::new())),
            Pack(children) => nodes.push(new_mim(MimKind::Pack, children, 0, String::new())),
            Tuple(children) => nodes.push(new_mim(MimKind::Tuple, children, 0, String::new())),
            Extract(children) => nodes.push(new_mim(MimKind::Extract, children, 0, String::new())),
            Insert(children) => nodes.push(new_mim(MimKind::Insert, children, 0, String::new())),
            Rule(children) => nodes.push(new_mim(MimKind::Rule, children, 0, String::new())),
            Inj(children) => nodes.push(new_mim(MimKind::Inj, children, 0, String::new())),
            Merge(children) => nodes.push(new_mim(MimKind::Merge, children, 0, String::new())),
            Axm(children) => nodes.push(new_mim(MimKind::Axm, children, 0, String::new())),
            Match(children) => nodes.push(new_mim(MimKind::Match, children, 0, String::new())),
            Proxy(children) => nodes.push(new_mim(MimKind::Proxy, children, 0, String::new())),

            Join(children) => nodes.push(new_mim(MimKind::Join, children, 0, String::new())),
            Meet(children) => nodes.push(new_mim(MimKind::Meet, children, 0, String::new())),
            Bot(child) => nodes.push(new_mim(MimKind::Bot, &[*child], 0, String::new())),
            Top(child) => nodes.push(new_mim(MimKind::Top, &[*child], 0, String::new())),
            Arr(children) => nodes.push(new_mim(MimKind::Arr, children, 0, String::new())),
            Sigma(children) => nodes.push(new_mim(MimKind::Sigma, children, 0, String::new())),
            Cn(child) => nodes.push(new_mim(MimKind::Cn, &[*child], 0, String::new())),
            Pi(children) => nodes.push(new_mim(MimKind::Pi, children, 0, String::new())),
            Idx(child) => nodes.push(new_mim(MimKind::Idx, &[*child], 0, String::new())),
            Hole(child) => nodes.push(new_mim(MimKind::Hole, &[*child], 0, String::new())),
            Type(child) => nodes.push(new_mim(MimKind::Type, &[*child], 0, String::new())),
            Reform(child) => nodes.push(new_mim(MimKind::Type, &[*child], 0, String::new())),

            Num(n) => nodes.push(new_mim(MimKind::Num, &[], *n, String::new())),
            Symbol(s) => nodes.push(new_mim(MimKind::Symbol, &[], 0, s.clone())),
        }
    }

    RewriteResult { value: nodes }
}
