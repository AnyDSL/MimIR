use crate::Mim::*;
use crate::rules::*;
use egg::*;
use ffi::{MimKind, MimNode};

mod rules;

pub fn equality_saturate(sexpr: &str) -> Vec<MimNode> {
    let rules: &[Rewrite<Mim, MimAnalysis>] = &rules();

    // TODO: if we have a series of sexpr's like multiple lambdas in a row,
    // only the first lambda is parsed into an egraph here.
    // gotta find a way that all of them are added to the egraph.
    let runner = Runner::<Mim, MimAnalysis, ()>::default()
        .with_expr(&sexpr.parse().unwrap())
        .run(rules);

    // TODO: this is useful now to see the egraph but should be removed later
    // runner
    //     .egraph
    //     .dot()
    //     .to_png("./examples/core/normalize_add.png")
    //     .expect("Failed to create dot graph.");

    let extractor = Extractor::new(&runner.egraph, AstSize);
    let (_best_cost, best_expr) = extractor.find_best(runner.roots[0]);

    // TODO: remove after development
    // println!("The best cost is: {}", best_cost);
    // println!("Post rewrite: {}", best_expr);

    rexpr_to_vec(best_expr)
}

#[cxx::bridge]
pub mod ffi {

    #[derive(Debug)]
    enum MimKind {
        Lam,
        Con,
        App,
        Var,
        Lit,
        Arr,
        Tuple,
        Extract,
        Ins,
        Sigma,
        Cn,
        Pi,
        Idx,
        Hole,
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

    extern "Rust" {
        fn equality_saturate(sexpr: &str) -> Vec<MimNode>;
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

fn rexpr_to_vec(rexpr: RecExpr<Mim>) -> Vec<MimNode> {
    let mut nodes = Vec::new();

    for node in rexpr.as_ref() {
        match node {
            Lam(children) => nodes.push(new_mim(MimKind::Lam, children, 0, String::new())),
            Con(children) => nodes.push(new_mim(MimKind::Con, children, 0, String::new())),
            App(children) => nodes.push(new_mim(MimKind::App, children, 0, String::new())),

            Mim::Var(children) => nodes.push(new_mim(MimKind::Var, children, 0, String::new())),
            Lit(children) => nodes.push(new_mim(MimKind::Lit, children, 0, String::new())),

            Arr(children) => nodes.push(new_mim(MimKind::Arr, children, 0, String::new())),
            Tuple(children) => nodes.push(new_mim(MimKind::Tuple, children, 0, String::new())),
            Extract(children) => nodes.push(new_mim(MimKind::Extract, children, 0, String::new())),
            Ins(children) => nodes.push(new_mim(MimKind::Ins, children, 0, String::new())),

            Sigma(children) => nodes.push(new_mim(MimKind::Sigma, children, 0, String::new())),
            Cn(child) => nodes.push(new_mim(MimKind::Cn, &[*child], 0, String::new())),
            Pi(children) => nodes.push(new_mim(MimKind::Pi, children, 0, String::new())),
            Idx(child) => nodes.push(new_mim(MimKind::Idx, &[*child], 0, String::new())),
            Hole(child) => nodes.push(new_mim(MimKind::Hole, &[*child], 0, String::new())),

            Num(n) => nodes.push(new_mim(MimKind::Num, &[], *n, String::new())),
            Symbol(s) => nodes.push(new_mim(MimKind::Symbol, &[], 0, s.clone())),
        }
    }

    nodes
}
