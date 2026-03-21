use crate::Mim::*;
use crate::rules::*;
use egg::*;
use ffi::{MimKind, MimNode, RewriteResult, RuleSet};

mod rules;

pub fn equality_saturate(sexpr: &str, rulesets: Vec<RuleSet>) -> Vec<RewriteResult> {
    let rules: &[Rewrite<Mim, MimAnalysis>] = &rules(rulesets);

    // TODO: this is a naive split that only works on linux (win: split on \r\n\r\n)
    let sexprs: Vec<&str> = sexpr.split("\n\n").collect();
    let mut rewritten_sexprs: Vec<RewriteResult> = Vec::new();

    for sexpr in sexprs {
        if sexpr.replace("\r", "").replace("\n", "").is_empty() {
            continue;
        }

        let runner = Runner::<Mim, MimAnalysis, ()>::default()
            .with_expr(&sexpr.parse().unwrap())
            .run(rules);

        let extractor = Extractor::new(&runner.egraph, AstSize);
        let (_best_cost, best_expr) = extractor.find_best(runner.roots[0]);

        rewritten_sexprs.push(rexpr_to_res(best_expr));
    }

    rewritten_sexprs
}

pub fn mim_node_str(node: MimNode) -> String {
    format!("{:?}", node)
}

pub fn pretty(sexpr: &str, line_len: usize) -> String {
    // TODO: this is a naive split that only works on linux (win: split on \r\n\r\n)
    let sexprs: Vec<&str> = sexpr.split("\n\n").collect();
    let mut res = String::new();

    for sexpr in sexprs {
        if sexpr.replace("\r", "").replace("\n", "").is_empty() {
            continue;
        }

        let parsed: RecExpr<Mim> = sexpr.parse().unwrap();
        res.push_str(parsed.pretty(line_len).as_str());
        res.push_str("\n\n");
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
        Ins,
        Arr,
        Sigma,
        Cn,
        Pi,
        Idx,
        Hole,
        Type,
        Num,
        Symbol,
    }

    #[derive(Debug)]
    struct MimNode {
        kind: MimKind,
        children: Vec<u32>,
        num: i32,
        symbol: String,
    }

    #[derive(Debug)]
    struct RewriteResult {
        value: Vec<MimNode>,
    }

    extern "Rust" {
        fn equality_saturate(sexpr: &str, rulesets: Vec<RuleSet>) -> Vec<RewriteResult>;
        fn mim_node_str(node: MimNode) -> String;
        fn pretty(sexpr: &str, line_len: usize) -> String;
    }
}

fn new_mim(kind: MimKind, children: &[Id], num: i32, symbol: String) -> MimNode {
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
            Ins(children) => nodes.push(new_mim(MimKind::Ins, children, 0, String::new())),

            Arr(children) => nodes.push(new_mim(MimKind::Arr, children, 0, String::new())),
            Sigma(children) => nodes.push(new_mim(MimKind::Sigma, children, 0, String::new())),
            Cn(child) => nodes.push(new_mim(MimKind::Cn, &[*child], 0, String::new())),
            Pi(children) => nodes.push(new_mim(MimKind::Pi, children, 0, String::new())),
            Idx(child) => nodes.push(new_mim(MimKind::Idx, &[*child], 0, String::new())),
            Hole(child) => nodes.push(new_mim(MimKind::Hole, &[*child], 0, String::new())),
            Type(child) => nodes.push(new_mim(MimKind::Type, &[*child], 0, String::new())),

            Num(n) => nodes.push(new_mim(MimKind::Num, &[], *n, String::new())),
            Symbol(s) => nodes.push(new_mim(MimKind::Symbol, &[], 0, s.clone())),
        }
    }

    RewriteResult { value: nodes }
}
