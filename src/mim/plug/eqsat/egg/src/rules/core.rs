use crate::rules::*;
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

// TODO:
/* core.bit1 */
/* core.bit2 */

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

// TODO: finish today and then look into constant folding some more (math.rs and prop.rs)

/* core.wrap */

fn wrap_add0() -> Rewrite<Mim, MimAnalysis> {
    let pat: Pattern<Mim> = "(app (app %core.wrap.add ?mode) (tuple ?a (lit 0 ?type)))"
        .parse()
        .unwrap();
    let outpat: Pattern<Mim> = "?a".parse().unwrap();

    Rewrite::new("wrap_add0", pat, outpat).unwrap()
}

// TODO: how to get the type for (lit 2 ?type)
// fn wrap_add_equal() -> Rewrite<Mim, MimAnalysis> {
//     let pat: Pattern<Mim> = "(app (app %core.wrap.add ?mode) (tuple ?a ?a))"
//         .parse()
//         .unwrap();
//     let outpat: Pattern<Mim> = "(app (app %core.wrap.mul ?mode) (tuple ?a (lit 2 ?type))".parse().unwrap();
//
//     Rewrite::new("wrap_add_equal", pat, outpat).unwrap()
// }

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

// TODO: how to get the type for (lit 0 ?type)
// fn wrap_sub_equal() -> Rewrite<Mim, MimAnalysis> {
//     let pat: Pattern<Mim> = "(app (app %core.wrap.sub ?mode) (tuple ?a ?a))"
//         .parse()
//         .unwrap();
//     let outpat: Pattern<Mim> = "(lit 0 ?type)".parse().unwrap();
//
//     Rewrite::new("wrap_sub_equal", pat, outpat).unwrap()
// }

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

// TODO: figure out how to get ?type for the output literal
// fn div_sdiv_equal() -> Rewrite<Mim, MimAnalysis> {
//     let pat: Pattern<Mim> = "(app %core.div.sdiv (tuple ?mem (lit 0 ?type) ?a))"
//         .parse()
//         .unwrap();
//     let outpat: Pattern<Mim> = "(lit 0 ?type)".parse().unwrap();
//
//     Rewrite::new("div_sdiv0", pat, outpat).unwrap()
// }
//
// fn div_udiv_equal() -> Rewrite<Mim, MimAnalysis> {
//     let pat: Pattern<Mim> = "(app %core.div.udiv (tuple ?mem (lit 0 ?type) ?a))"
//         .parse()
//         .unwrap();
//     let outpat: Pattern<Mim> = "(lit 0 ?type)".parse().unwrap();
//
//     Rewrite::new("div_udiv0", pat, outpat).unwrap()
// }
//
// fn div_srem_equal() -> Rewrite<Mim, MimAnalysis> {
//     let pat: Pattern<Mim> = "(app %core.div.srem (tuple ?mem (lit 0 ?type) ?a))"
//         .parse()
//         .unwrap();
//     let outpat: Pattern<Mim> = "(lit 0 ?type)".parse().unwrap();
//
//     Rewrite::new("div_srem0", pat, outpat).unwrap()
// }
//
// fn div_urem_equal() -> Rewrite<Mim, MimAnalysis> {
//     let pat: Pattern<Mim> = "(app %core.div.urem (tuple ?mem (lit 0 ?type) ?a))"
//         .parse()
//         .unwrap();
//     let outpat: Pattern<Mim> = "(lit 0 ?type)".parse().unwrap();
//
//     Rewrite::new("div_urem0", pat, outpat).unwrap()
// }

/* helpers */

fn idx_size(type_: &Mim) -> i32 {
    if let Symbol(s) = type_ {
        match s.as_str() {
            "Bool" => return 1,
            "I1" => return 1,
            "I8" => return 8,
            "I16" => return 16,
            "I32" => return 32,
            "I64" => return 64,
            _ => panic!("expected idx type"),
        };
    } else if let Idx(_s) = type_ {
        // TODO:
        return 0;
    }
    panic!("expected idx type");
}

fn new_const(val: Option<Mim>, type_: Option<Mim>) -> Option<Const> {
    Some(Const { val, type_ })
}

fn bool_lit(tt: bool) -> Option<Const> {
    let ret_val = if tt { "tt" } else { "ff" }.to_string();
    new_const(Some(Symbol(ret_val)), None)
}

fn nat_lit(n: i32) -> Option<Const> {
    new_const(Some(Num(n)), None)
}

/* constant folding */

pub fn fold_core(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<Const> {
    if let Lit(l) = enode
        && let Some(v) = egraph[l[0]].nodes.first()
    {
        // Case 1: typed literal e.g. (lit 4 I8)
        if l.len() == 2
            && let Some(t) = egraph[l[1]].nodes.first()
        {
            return new_const(Some(v.clone()), Some(t.clone()));
        }
        // Case 2: untyped literal e.g. (lit 5)
        return new_const(Some(v.clone()), None);
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

fn fold_nat(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<Const> {
    let c = |id: &Id| egraph[*id].data.constant.clone();

    if let App([callee, arg]) = enode
        && let Some(s) = find_node!(egraph, callee, Symbol(s) => s)
        && let Some(t) = find_node!(egraph, arg, Tuple(t) => t)
        && let [t1, t2] = &**t
        && let Some(Num(n1)) = c(t1)?.val
        && let Some(Num(n2)) = c(t2)?.val
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

// TODO: gather more info about how idx literals are printed
// to ensure that the conditional and the plusminus stuff is going to work
fn fold_icmp(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<Const> {
    let c = |id: &Id| egraph[*id].data.constant.clone();

    if let App([callee, arg]) = enode
        && let Some(s) = find_node!(egraph, callee, Symbol(s) => s)
        && let Some(t) = find_node!(egraph, arg, Tuple(t) => t)
        && let [t1, t2] = &**t
        && let Some(Num(n1)) = c(t1)?.val
        && let Some(i1) = c(t1)?.type_
        && let Some(Num(n2)) = c(t2)?.val
        && let Some(i2) = c(t2)?.type_
    {
        // TODO: the above conditional is true for any application
        // of a symbol to a tuple of two typed literals so we can't
        // just assume that these types will be idx types
        let size1 = idx_size(&i1);
        let size2 = idx_size(&i2);
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

fn fold_ncmp(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<Const> {
    let c = |id: &Id| egraph[*id].data.constant.clone();

    if let App([callee, arg]) = enode
        && let Some(s) = find_node!(egraph, callee, Symbol(s) => s)
        && let Some(t) = find_node!(egraph, arg, Tuple(t) => t)
        && let [t1, t2] = &**t
        && let Some(Num(n1)) = c(t1)?.val
        && let Some(Num(n2)) = c(t2)?.val
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

fn fold_shr(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<Const> {
    let c = |id: &Id| egraph[*id].data.constant.clone();

    if let App([callee, arg]) = enode
        && let Some(s) = find_node!(egraph, callee, Symbol(s) => s)
        && let Some(t) = find_node!(egraph, arg, Tuple(t) => t)
        && let [t1, t2] = &**t
        && let Some(Num(n1)) = c(t1)?.val
        && let Some(Num(n2)) = c(t2)?.val
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

fn fold_wrap(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<Const> {
    let c = |id: &Id| egraph[*id].data.constant.clone();

    // (app (app %core.wrap.(add,sub,mul,shl) [mode: Nat]) <<2; Idx s>>)
    if let App([callee, arg]) = enode
        && let Some([name, mode]) = find_node!(egraph, callee, App([name, mode]) => [name, mode])
        && let Some(s) = find_node!(egraph, name, Symbol(s) => s)
        && let Some(t) = find_node!(egraph, arg, Tuple(t) => t)
        && let [t1, t2] = &**t
        && let Some(Num(_m)) = c(mode)?.val
        && let Some(Num(n1)) = c(t1)?.val
        && let Some(Num(n2)) = c(t2)?.val
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

fn fold_div(egraph: &mut EGraph<Mim, MimAnalysis>, enode: &Mim) -> Option<Const> {
    let c = |id: &Id| egraph[*id].data.constant.clone();

    // (app %core.div.(udiv,sdiv,urem,srem) [%mem.M 0, <<2; Idx s>>])
    if let App([callee, arg]) = enode
        && let Some(s) = find_node!(egraph, callee, Symbol(s) => s)
        && let Some(t) = find_node!(egraph, arg, Tuple(t) => t)
        && let [_t1, t2] = &**t
        && let Some(vals) = find_node!(egraph, t2, Tuple(vals) => vals)
        && let [v1, v2] = &**vals
        && let Some(Num(n1)) = c(v1)?.val
        && let Some(Num(n2)) = c(v2)?.val
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
