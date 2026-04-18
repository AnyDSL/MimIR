use crate::mim_egg::Mim;
use crate::mim_egg::Mim::*;
use crate::{equality_saturate, mim_node_str, pretty};
use bridge::{MimKind, MimNode, RewriteResult};
use egg::{Id, RecExpr};

#[cxx::bridge]
pub mod bridge {
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

pub fn rec_expr_to_res(rec_expr: RecExpr<Mim>) -> RewriteResult {
    let mut nodes = Vec::new();

    for node in rec_expr.as_ref() {
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
