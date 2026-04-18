use crate::find_node;
use crate::mim_egg::Mim;
use crate::mim_egg::Mim::*;
use crate::mim_egg::analysis::{AnalysisData, MimAnalysis};
use egg::*;
use std::cmp::max;

pub fn rules() -> Vec<Rewrite<Mim, MimAnalysis>> {
    let rules = vec![
        nat_add0(),
        nat_add_same(),
        nat_commute_add(),
        nat_sub0(),
        nat_sub_same(),
        nat_mul0(),
        nat_mul1(),
        nat_commute_mul(),
        icmp_equal(),
        icmp_not_equal(),
        icmp_true(),
        icmp_false(),
        ncmp_equal(),
        ncmp_not_equal(),
        ncmp_true(),
        ncmp_false(),
        shr_arith_amount0(),
        shr_arith_val0(),
        shr_logical_amount0(),
        shr_logical_val0(),
        wrap_add0(),
        wrap_commute_add(),
        wrap_sub0(),
        wrap_mul0(),
        wrap_mul1(),
        wrap_commute_mul(),
        wrap_shl_val0(),
        wrap_shl_amount0(),
        div_sdiv0(),
        div_sdiv1(),
        div_udiv0(),
        div_udiv1(),
        div_srem0(),
        div_srem1(),
        div_urem0(),
        div_urem1(),
    ];

    rules
}

/* core.nat */

fn nat_add0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.nat.add (tuple (lit 0) ?e))".parse().unwrap();
    let outpat: Pattern<Mim> = "?e".parse().unwrap();

    Rewrite::new("nat_add0", pat, outpat).unwrap()
}

fn nat_add_same() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.nat.add (tuple ?a ?a))".parse().unwrap();
    let outpat: Pattern<Mim> = "(app %core.nat.mul (tuple (lit 2) ?a))".parse().unwrap();

    Rewrite::new("nat_add_same", pat, outpat).unwrap()
}

fn nat_commute_add() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.nat.add (tuple ?a ?b))".parse().unwrap();
    let outpat: Pattern<Mim> = "(app %core.nat.add (tuple ?b ?a))".parse().unwrap();

    Rewrite::new("nat_commute_add", pat, outpat).unwrap()
}

fn nat_sub0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.nat.sub (tuple ?e (lit 0)))".parse().unwrap();
    let outpat: Pattern<Mim> = "?e".parse().unwrap();

    Rewrite::new("nat_sub0", pat, outpat).unwrap()
}

fn nat_sub_same() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.nat.sub (tuple ?a ?a))".parse().unwrap();
    let outpat: Pattern<Mim> = "(lit 0)".parse().unwrap();

    Rewrite::new("nat_sub_same", pat, outpat).unwrap()
}

fn nat_mul0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.nat.mul (tuple (lit 0) ?e))".parse().unwrap();
    let outpat: Pattern<Mim> = "(lit 0)".parse().unwrap();

    Rewrite::new("nat_mul0", pat, outpat).unwrap()
}

fn nat_mul1() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.nat.mul (tuple (lit 1) ?e))".parse().unwrap();
    let outpat: Pattern<Mim> = "?e".parse().unwrap();

    Rewrite::new("nat_mul1", pat, outpat).unwrap()
}

fn nat_commute_mul() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.nat.mul (tuple ?a ?b))".parse().unwrap();
    let outpat: Pattern<Mim> = "(app %core.nat.mul (tuple ?b ?a))".parse().unwrap();

    Rewrite::new("nat_commute_mul", pat, outpat).unwrap()
}

/* core.icmp */

fn icmp_equal() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.icmp.e (tuple ?a ?a))".parse().unwrap();
    let outpat: Pattern<Mim> = "(lit tt)".parse().unwrap();

    Rewrite::new("icmp_equal", pat, outpat).unwrap()
}

fn icmp_not_equal() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.icmp.ne (tuple ?a ?a))".parse().unwrap();
    let outpat: Pattern<Mim> = "(lit ff)".parse().unwrap();

    Rewrite::new("icmp_not_equal", pat, outpat).unwrap()
}

fn icmp_true() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.icmp.t (tuple ?a ?b))".parse().unwrap();
    let outpat: Pattern<Mim> = "(lit tt)".parse().unwrap();

    Rewrite::new("icmp_true", pat, outpat).unwrap()
}

fn icmp_false() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.icmp.f (tuple ?a ?b))".parse().unwrap();
    let outpat: Pattern<Mim> = "(lit ff)".parse().unwrap();

    Rewrite::new("icmp_false", pat, outpat).unwrap()
}

/* core.ncmp */

fn ncmp_equal() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.ncmp.e (tuple ?a ?a))".parse().unwrap();
    let outpat: Pattern<Mim> = "(lit tt)".parse().unwrap();

    Rewrite::new("ncmp_equal", pat, outpat).unwrap()
}

fn ncmp_not_equal() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.ncmp.ne (tuple ?a ?a))".parse().unwrap();
    let outpat: Pattern<Mim> = "(lit ff)".parse().unwrap();

    Rewrite::new("ncmp_not_equal", pat, outpat).unwrap()
}

fn ncmp_true() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.ncmp.t (tuple ?a ?b))".parse().unwrap();
    let outpat: Pattern<Mim> = "(lit tt)".parse().unwrap();

    Rewrite::new("ncmp_true", pat, outpat).unwrap()
}

fn ncmp_false() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.ncmp.f (tuple ?a ?b))".parse().unwrap();
    let outpat: Pattern<Mim> = "(lit ff)".parse().unwrap();

    Rewrite::new("ncmp_false", pat, outpat).unwrap()
}

/* core.shr */

fn shr_arith_val0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.shr.a (tuple (lit 0 ?type) ?a))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(lit 0 ?type)".parse().unwrap();

    Rewrite::new("shr_arith_val0", pat, outpat).unwrap()
}

fn shr_arith_amount0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.shr.a (tuple ?a (lit 0 ?type)))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "?a".parse().unwrap();

    Rewrite::new("shr_arith_amount0", pat, outpat).unwrap()
}

fn shr_logical_val0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.shr.l (tuple (lit 0 ?type) ?a))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(lit 0 ?type)".parse().unwrap();

    Rewrite::new("shr_logical_val0", pat, outpat).unwrap()
}

fn shr_logical_amount0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.shr.l (tuple ?a (lit 0 ?type)))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "?a".parse().unwrap();

    Rewrite::new("shr_logical_amount0", pat, outpat).unwrap()
}

/* core.wrap */

fn wrap_add0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app (app %core.wrap.add ?mode) (tuple ?a (lit 0 ?type)))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "?a".parse().unwrap();

    Rewrite::new("wrap_add0", pat, outpat).unwrap()
}

fn wrap_commute_add() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app (app %core.wrap.add ?mode) (tuple ?a ?b))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(app (app %core.wrap.add ?mode) (tuple ?b ?a))"
        .parse()
        .unwrap();

    Rewrite::new("wrap_commute_add", pat, outpat).unwrap()
}

fn wrap_sub0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app (app %core.wrap.sub ?mode) (tuple ?a (lit 0 ?type)))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "?a".parse().unwrap();

    Rewrite::new("wrap_sub0", pat, outpat).unwrap()
}

fn wrap_mul0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app (app %core.wrap.mul ?mode) (tuple ?a (lit 0 ?type)))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(lit 0 ?type)".parse().unwrap();

    Rewrite::new("wrap_mul0", pat, outpat).unwrap()
}

fn wrap_mul1() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app (app %core.wrap.mul ?mode) (tuple ?a (lit 1 ?type)))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "?a".parse().unwrap();

    Rewrite::new("wrap_mul1", pat, outpat).unwrap()
}

fn wrap_commute_mul() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app (app %core.wrap.mul ?mode) (tuple ?a ?b))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(app (app %core.wrap.mul ?mode) (tuple ?b ?a))"
        .parse()
        .unwrap();

    Rewrite::new("wrap_commute_mul", pat, outpat).unwrap()
}

fn wrap_shl_val0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app (app %core.wrap.shl ?mode) (tuple (lit 0 ?type) ?a))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(lit 0 ?type)".parse().unwrap();

    Rewrite::new("wrap_shl_val0", pat, outpat).unwrap()
}

fn wrap_shl_amount0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app (app %core.wrap.shl ?mode) (tuple ?a (lit 0 ?type)))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "?a".parse().unwrap();

    Rewrite::new("wrap_shl_amount0", pat, outpat).unwrap()
}

/* core.div */

fn div_sdiv0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.div.sdiv (tuple ?mem (lit 0 ?type) ?a))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(tuple ?mem (lit 0 ?type))".parse().unwrap();

    Rewrite::new("div_sdiv0", pat, outpat).unwrap()
}

fn div_udiv0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.div.udiv (tuple ?mem (lit 0 ?type) ?a))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(tuple ?mem (lit 0 ?type))".parse().unwrap();

    Rewrite::new("div_udiv0", pat, outpat).unwrap()
}

fn div_srem0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.div.srem (tuple ?mem (lit 0 ?type) ?a))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(tuple ?mem (lit 0 ?type))".parse().unwrap();

    Rewrite::new("div_srem0", pat, outpat).unwrap()
}

fn div_urem0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.div.urem (tuple ?mem (lit 0 ?type) ?a))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(tuple ?mem (lit 0 ?type))".parse().unwrap();

    Rewrite::new("div_urem0", pat, outpat).unwrap()
}

fn div_sdiv1() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.div.sdiv (tuple ?mem ?a (lit 1 ?type)))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(tuple ?mem ?a)".parse().unwrap();

    Rewrite::new("div_sdiv1", pat, outpat).unwrap()
}

fn div_udiv1() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.div.udiv (tuple ?mem ?a (lit 1 ?type)))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(tuple ?mem (lit 1 ?type))".parse().unwrap();

    Rewrite::new("div_udiv1", pat, outpat).unwrap()
}

fn div_srem1() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.div.srem (tuple ?mem ?a (lit 1 ?type)))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(tuple ?mem (lit 0 ?type))".parse().unwrap();

    Rewrite::new("div_srem1", pat, outpat).unwrap()
}

fn div_urem1() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app %core.div.urem (tuple ?mem ?a (lit 1 ?type)))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "(tuple ?mem (lit 0 ?type))".parse().unwrap();

    Rewrite::new("div_urem1", pat, outpat).unwrap()
}

/* helpers */

fn idx_size(type_: &Mim) -> i32 {
    if let Symbol(s) = type_ {
        match s.as_str() {
            "Bool" => return 1,
            "I8" => return 8,
            "I16" => return 16,
            "I32" => return 32,
            "I64" => return 64,
            _ => panic!("expected Idx type"),
        };
    } else if let Idx(_s) = type_ {
        // TODO:
        return 0;
    }
    panic!("expected Idx type");
}

fn new_const(val: Mim, type_: Mim) -> Option<CoreConst> {
    Some(CoreConst { val, type_ })
}

fn bool_lit(tt: bool) -> Option<CoreConst> {
    let ret_val = if tt { "tt" } else { "ff" }.to_string();
    let bool = "Bool".to_string();
    new_const(Symbol(ret_val), Symbol(bool))
}

fn nat_lit(n: i64) -> Option<CoreConst> {
    let nat = "Nat".to_string();
    new_const(Num(n), Symbol(nat))
}

/* constant folding */

pub type CoreData = Option<CoreConst>;

#[derive(Debug, Clone)]
pub struct CoreConst {
    val: Mim,
    type_: Mim,
}

pub fn core_merge(a: &mut AnalysisData, b: AnalysisData) -> DidMerge {
    if a.core_data.is_none() && b.core_data.is_some() {
        a.core_data = b.core_data;
        DidMerge(true, false)
    } else {
        DidMerge(false, false)
    }
}

pub fn core_make(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim, _id: Id) -> AnalysisData {
    AnalysisData {
        core_data: fold_core(egraph, enode),
    }
}

pub fn core_modify(egraph: &mut EGraph<Mim, MimAnalysis>, id: Id) {
    if let Some(CoreConst { val: c, type_: t }) = egraph[id].data.core_data.clone() {
        let const_id = egraph.add(c);
        let type_id = egraph.add(t);
        let lit_id = egraph.add(Lit([const_id, type_id]));
        egraph.union(id, lit_id);
    }
}

// Can be used to create conditional rewrite rules like (foo ?a) => (bar ?a) if is_const(var("?a"))
fn _is_const(v: egg::Var) -> impl Fn(&mut EGraph<Mim, MimAnalysis>, Id, &Subst) -> bool {
    move |eg, _, subst| eg[subst[v]].data.core_data.is_some()
}

pub fn fold_core(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<CoreConst> {
    if let Lit([v, t]) = enode
        && let Some(v) = egraph[*v].nodes.first()
        && let Some(t) = egraph[*t].nodes.first()
    {
        return new_const(v.clone(), t.clone());
    }

    if let Some(folded) = fold_nat(egraph, enode) {
        return Some(folded);
    } else if let Some(folded) = fold_icmp(egraph, enode) {
        return Some(folded);
    } else if let Some(folded) = fold_ncmp(egraph, enode) {
        return Some(folded);
    } else if let Some(folded) = fold_shr(egraph, enode) {
        return Some(folded);
    } else if let Some(folded) = fold_wrap(egraph, enode) {
        return Some(folded);
    } else if let Some(folded) = fold_div(egraph, enode) {
        return Some(folded);
    }

    None
}

fn fold_nat(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<CoreConst> {
    let c = |id: &Id| egraph[*id].data.core_data.clone();

    if let App([callee, arg]) = enode
        && let Some(s) = find_node!(egraph, callee, Symbol(s) => s)
        && let Some(t) = find_node!(egraph, arg, Tuple(t) => t)
        && let [t1, t2] = &**t
        && let Num(n1) = c(t1)?.val
        && let Num(n2) = c(t2)?.val
    {
        match s.as_str() {
            "%core.nat.add" => return nat_lit(n1 + n2),
            "%core.nat.sub" => return nat_lit(max(n1 - n2, 0)),
            "%core.nat.mul" => return nat_lit(n1 * n2),
            _ => (),
        }
    }

    None
}

fn fold_icmp(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<CoreConst> {
    let c = |id: &Id| egraph[*id].data.core_data.clone();

    if let App([callee, arg]) = enode
        && let Some(s) = find_node!(egraph, callee, Symbol(s) => s)
        && let Some(t) = find_node!(egraph, arg, Tuple(t) => t)
        && let [e1, e2] = &**t
        && let CoreConst {
            val: Num(n1),
            type_: t1,
        } = c(e1)?
        && let CoreConst {
            val: Num(n2),
            type_: t2,
        } = c(e2)?
    {
        // TODO: the above conditional is true for any application
        // of a symbol to a tuple of two typed literals so we can't
        // just assume that these types will be idx types
        let size1 = idx_size(&t1);
        let size2 = idx_size(&t2);
        if size1 != size2 {
            panic!("icmp: idx size mismatch")
        };

        let plusminus = (n1 >> (size1 - 1)) == 0 && (n2 >> (size2 - 1)) == 1;
        let minusplus = (n1 >> (size1 - 1)) == 1 && (n2 >> (size2 - 1)) == 0;

        match s.as_str() {
            "%core.icmp.Xygle" => return bool_lit(plusminus),
            "%core.icmp.xYgle" => return bool_lit(minusplus),
            "%core.icmp.xyGle" => return bool_lit(n1 > n2 && !minusplus),
            "%core.icmp.xygLe" => return bool_lit(n1 < n2 && !plusminus),
            "%core.icmp.xyglE" => return bool_lit(n1 == n2),
            _ => (),
        }
    }

    None
}

fn fold_ncmp(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<CoreConst> {
    let c = |id: &Id| egraph[*id].data.core_data.clone();

    if let App([callee, arg]) = enode
        && let Some(s) = find_node!(egraph, callee, Symbol(s) => s)
        && let Some(t) = find_node!(egraph, arg, Tuple(t) => t)
        && let [t1, t2] = &**t
        && let Num(n1) = c(t1)?.val
        && let Num(n2) = c(t2)?.val
    {
        match s.as_str() {
            "%core.ncmp.e" => return bool_lit(n1 == n2),
            "%core.ncmp.ne" => return bool_lit(n1 != n2),
            "%core.ncmp.l" => return bool_lit(n1 <= n2),
            "%core.ncmp.le" => return bool_lit(n1 < n2),
            "%core.ncmp.g" => return bool_lit(n1 > n2),
            "%core.ncmp.ge" => return bool_lit(n1 >= n2),
            _ => (),
        }
    }

    None
}

fn fold_shr(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<CoreConst> {
    let c = |id: &Id| egraph[*id].data.core_data.clone();

    if let App([callee, arg]) = enode
        && let Some(s) = find_node!(egraph, callee, Symbol(s) => s)
        && let Some(t) = find_node!(egraph, arg, Tuple(t) => t)
        && let [t1, t2] = &**t
        && let Num(n1) = c(t1)?.val
        && let Num(n2) = c(t2)?.val
    {
        // TODO: check if shift amount exceeds width
        // and differentiate between signed and unsigned
        match s.as_str() {
            "%core.shr.a" => return nat_lit(n1 >> n2),
            "%core.shr.l" => return nat_lit(n1 >> n2),
            _ => (),
        }
    }

    None
}

fn fold_wrap(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<CoreConst> {
    let c = |id: &Id| egraph[*id].data.core_data.clone();

    // (app (app %core.wrap.(add,sub,mul,shl) [mode: Nat]) <<2; Idx s>>)
    if let App([callee, arg]) = enode
        && let Some([name, mode]) = find_node!(egraph, callee, App([name, mode]) => [name, mode])
        && let Some(s) = find_node!(egraph, name, Symbol(s) => s)
        && let Some(t) = find_node!(egraph, arg, Tuple(t) => t)
        && let [t1, t2] = &**t
        && let Num(_m) = c(mode)?.val
        && let Num(n1) = c(t1)?.val
        && let Num(n2) = c(t2)?.val
    {
        // TODO: utilize mode and implement wrapping
        match s.as_str() {
            "%core.wrap.add" => return nat_lit(n1 + n2),
            "%core.wrap.sub" => return nat_lit(max(n1 - n2, 0)),
            "%core.wrap.mul" => return nat_lit(n1 * n2),
            "%core.wrap.shl" => return nat_lit(n1 << n2),
            _ => (),
        }
    }

    None
}

fn fold_div(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<CoreConst> {
    let c = |id: &Id| egraph[*id].data.core_data.clone();

    // (app %core.div.(udiv,sdiv,urem,srem) [%mem.M 0, <<2; Idx s>>])
    if let App([callee, arg]) = enode
        && let Some(s) = find_node!(egraph, callee, Symbol(s) => s)
        && let Some(t) = find_node!(egraph, arg, Tuple(t) => t)
        && let [_t1, t2] = &**t
        && let Some(vals) = find_node!(egraph, t2, Tuple(vals) => vals)
        && let [v1, v2] = &**vals
        && let Num(n1) = c(v1)?.val
        && let Num(n2) = c(v2)?.val
    {
        // TODO: signed vs unsigned div
        match s.as_str() {
            "%core.div.udiv" => return nat_lit(n1 / n2),
            "%core.div.sdiv" => return nat_lit(n1 / n2),
            "%core.div.urem" => return nat_lit(n1 % n2),
            "%core.div.srem" => return nat_lit(n1 % n2),
            _ => (),
        }
    }

    None
}
