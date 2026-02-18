use crate::rules::*;

pub fn rules() -> Vec<Rewrite<Mim, MimAnalysis>> {
    let rules = vec![];

    rules
}

// fn add0() -> Rewrite<Mim, ()> {
//     let pat: Pattern<Mim> = "(app %core.nat.add (tuple (lit 0) ?e))".parse().unwrap();
//     let outpat: Pattern<Mim> = "?e".parse().unwrap();
//
//     Rewrite::new("add0", pat, outpat).unwrap()
// }
