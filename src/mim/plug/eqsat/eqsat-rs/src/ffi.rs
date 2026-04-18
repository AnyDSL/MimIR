use crate::mim_egg::Mim;
use crate::mim_slotted::MimSlotted;
use crate::{equality_saturate, mim_node_str, pretty};
use bridge::{MimKind, MimNode, RewriteResult};
use egg::{Id, RecExpr};
use slotted_egraphs::Id as IdSlotted;
use slotted_egraphs::RecExpr as RecExprSlotted;

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
        Reform,
        Cons,
        Nil,
        Num,
        Symbol,
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

fn new_mim(kind: MimKind, children: &[Id], num: Option<i64>, symbol: Option<String>) -> MimNode {
    let converted_ids = children.iter().map(|id| usize::from(*id) as u32).collect();

    MimNode {
        kind,
        children: converted_ids,
        num: num.unwrap_or_default(),
        symbol: symbol.unwrap_or_default(),
    }
}

pub fn rec_expr_to_res(rec_expr: RecExpr<Mim>) -> RewriteResult {
    let mut nodes = Vec::new();

    for node in rec_expr.as_ref() {
        match node {
            Mim::Let(children) => nodes.push(new_mim(MimKind::Let, children, None, None)),
            Mim::Lam(children) => nodes.push(new_mim(MimKind::Lam, children, None, None)),
            Mim::Con(children) => nodes.push(new_mim(MimKind::Con, children, None, None)),
            Mim::App(children) => nodes.push(new_mim(MimKind::App, children, None, None)),
            Mim::Var(children) => nodes.push(new_mim(MimKind::Var, children, None, None)),
            Mim::Lit(children) => nodes.push(new_mim(MimKind::Lit, children, None, None)),
            Mim::Pack(children) => nodes.push(new_mim(MimKind::Pack, children, None, None)),
            Mim::Tuple(children) => nodes.push(new_mim(MimKind::Tuple, children, None, None)),
            Mim::Extract(children) => nodes.push(new_mim(MimKind::Extract, children, None, None)),
            Mim::Insert(children) => nodes.push(new_mim(MimKind::Insert, children, None, None)),
            Mim::Rule(children) => nodes.push(new_mim(MimKind::Rule, children, None, None)),
            Mim::Inj(children) => nodes.push(new_mim(MimKind::Inj, children, None, None)),
            Mim::Merge(children) => nodes.push(new_mim(MimKind::Merge, children, None, None)),
            Mim::Axm(children) => nodes.push(new_mim(MimKind::Axm, children, None, None)),
            Mim::Match(children) => nodes.push(new_mim(MimKind::Match, children, None, None)),
            Mim::Proxy(children) => nodes.push(new_mim(MimKind::Proxy, children, None, None)),

            Mim::Join(children) => nodes.push(new_mim(MimKind::Join, children, None, None)),
            Mim::Meet(children) => nodes.push(new_mim(MimKind::Meet, children, None, None)),
            Mim::Bot(child) => nodes.push(new_mim(MimKind::Bot, &[*child], None, None)),
            Mim::Top(child) => nodes.push(new_mim(MimKind::Top, &[*child], None, None)),
            Mim::Arr(children) => nodes.push(new_mim(MimKind::Arr, children, None, None)),
            Mim::Sigma(children) => nodes.push(new_mim(MimKind::Sigma, children, None, None)),
            Mim::Cn(child) => nodes.push(new_mim(MimKind::Cn, &[*child], None, None)),
            Mim::Pi(children) => nodes.push(new_mim(MimKind::Pi, children, None, None)),
            Mim::Idx(child) => nodes.push(new_mim(MimKind::Idx, &[*child], None, None)),
            Mim::Hole(child) => nodes.push(new_mim(MimKind::Hole, &[*child], None, None)),
            Mim::Type(child) => nodes.push(new_mim(MimKind::Type, &[*child], None, None)),
            Mim::Reform(child) => nodes.push(new_mim(MimKind::Type, &[*child], None, None)),

            Mim::Num(n) => nodes.push(new_mim(MimKind::Num, &[], Some(*n), None)),
            Mim::Symbol(s) => nodes.push(new_mim(MimKind::Symbol, &[], None, Some(s.clone()))),
        }
    }

    RewriteResult { value: nodes }
}

fn new_mim_slotted(
    kind: MimKind,
    children: &[IdSlotted],
    num: Option<i64>,
    symbol: Option<String>,
) -> MimNode {
    let converted_ids = children.iter().map(|id| id.0 as u32).collect();

    MimNode {
        kind,
        children: converted_ids,
        num: num.unwrap_or_default(),
        symbol: symbol.unwrap_or_default(),
    }
}

pub fn rec_expr_to_res_slotted(rec_expr: RecExprSlotted<MimSlotted>) -> RewriteResult {
    let mut nodes = Vec::new();

    for child in rec_expr.children {
        match child.node {
            MimSlotted::Let(name, def, expr) => nodes.push(new_mim_slotted(
                MimKind::Let,
                &[name.elem.id, def.id, expr.id],
                None,
                None,
            )),
            MimSlotted::Lam(ext, name, var_name, dom, codom, filter, body) => {
                nodes.push(new_mim_slotted(
                    MimKind::Lam,
                    &[
                        ext.id,
                        name.id,
                        var_name.elem.id,
                        dom.id,
                        codom.id,
                        filter.id,
                        body.id,
                    ],
                    None,
                    None,
                ))
            }
            MimSlotted::Con(ext, name, var_name, dom, filter, body) => nodes.push(new_mim_slotted(
                MimKind::Con,
                &[
                    ext.id,
                    name.id,
                    var_name.elem.id,
                    dom.id,
                    filter.id,
                    body.id,
                ],
                None,
                None,
            )),
            MimSlotted::App(callee, arg) => nodes.push(new_mim_slotted(
                MimKind::App,
                &[callee.id, arg.id],
                None,
                None,
            )),
            MimSlotted::Var(_slot) => nodes.push(new_mim_slotted(MimKind::Var, &[], None, None)),
            MimSlotted::Lit(val, type_) => nodes.push(new_mim_slotted(
                MimKind::Lit,
                &[val.id, type_.id],
                None,
                None,
            )),
            MimSlotted::Pack(arity, body) => nodes.push(new_mim_slotted(
                MimKind::Pack,
                &[arity.id, body.id],
                None,
                None,
            )),
            MimSlotted::Tuple(elem_cons) => {
                nodes.push(new_mim_slotted(MimKind::Tuple, &[elem_cons.id], None, None))
            }
            MimSlotted::Extract(tuple, index) => nodes.push(new_mim_slotted(
                MimKind::Extract,
                &[tuple.id, index.id],
                None,
                None,
            )),
            MimSlotted::Insert(tuple, index, value) => nodes.push(new_mim_slotted(
                MimKind::Insert,
                &[tuple.id, index.id, value.id],
                None,
                None,
            )),
            MimSlotted::Rule(name, meta_var, lhs, rhs, guard) => nodes.push(new_mim_slotted(
                MimKind::Rule,
                &[name.id, meta_var.id, lhs.id, rhs.id, guard.id],
                None,
                None,
            )),
            MimSlotted::Inj(type_, val) => nodes.push(new_mim_slotted(
                MimKind::Inj,
                &[type_.id, val.id],
                None,
                None,
            )),
            MimSlotted::Merge(type_, type_cons) => nodes.push(new_mim_slotted(
                MimKind::Merge,
                &[type_.id, type_cons.id],
                None,
                None,
            )),
            MimSlotted::Axm(name, type_) => nodes.push(new_mim_slotted(
                MimKind::Axm,
                &[name.id, type_.id],
                None,
                None,
            )),
            MimSlotted::Match(op_cons) => {
                nodes.push(new_mim_slotted(MimKind::Match, &[op_cons.id], None, None))
            }
            MimSlotted::Proxy(type_, pass, tag, op_cons) => nodes.push(new_mim_slotted(
                MimKind::Proxy,
                &[type_.id, pass.id, tag.id, op_cons.id],
                None,
                None,
            )),

            MimSlotted::Join(type_cons) => {
                nodes.push(new_mim_slotted(MimKind::Join, &[type_cons.id], None, None))
            }
            MimSlotted::Meet(type_cons) => {
                nodes.push(new_mim_slotted(MimKind::Meet, &[type_cons.id], None, None))
            }
            MimSlotted::Bot(type_) => {
                nodes.push(new_mim_slotted(MimKind::Bot, &[type_.id], None, None))
            }
            MimSlotted::Top(type_) => {
                nodes.push(new_mim_slotted(MimKind::Top, &[type_.id], None, None))
            }
            MimSlotted::Arr(arity, body) => nodes.push(new_mim_slotted(
                MimKind::Arr,
                &[arity.id, body.id],
                None,
                None,
            )),
            MimSlotted::Sigma(type_cons) => {
                nodes.push(new_mim_slotted(MimKind::Sigma, &[type_cons.id], None, None))
            }
            MimSlotted::Cn(domain) => {
                nodes.push(new_mim_slotted(MimKind::Cn, &[domain.id], None, None))
            }
            MimSlotted::Pi(domain, codomain) => nodes.push(new_mim_slotted(
                MimKind::Pi,
                &[domain.id, codomain.id],
                None,
                None,
            )),
            MimSlotted::Idx(size) => {
                nodes.push(new_mim_slotted(MimKind::Idx, &[size.id], None, None))
            }
            MimSlotted::Hole(type_) => {
                nodes.push(new_mim_slotted(MimKind::Hole, &[type_.id], None, None))
            }
            MimSlotted::Type(level) => {
                nodes.push(new_mim_slotted(MimKind::Type, &[level.id], None, None))
            }
            MimSlotted::Reform(meta_type) => {
                nodes.push(new_mim_slotted(MimKind::Type, &[meta_type.id], None, None))
            }

            MimSlotted::Cons(elem, next) => nodes.push(new_mim_slotted(
                MimKind::Cons,
                &[elem.id, next.id],
                None,
                None,
            )),
            MimSlotted::Nil() => nodes.push(new_mim_slotted(MimKind::Nil, &[], None, None)),

            MimSlotted::Num(n) => nodes.push(new_mim_slotted(MimKind::Num, &[], Some(n), None)),
            MimSlotted::Symbol(s) => nodes.push(new_mim_slotted(
                MimKind::Symbol,
                &[],
                None,
                Some(s.to_string()),
            )),
        }
    }

    RewriteResult { value: nodes }
}
