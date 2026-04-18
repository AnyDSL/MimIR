use eqsat::{equality_saturate, ffi::bridge::CostFn, ffi::bridge::RuleSet};
use std::fs;

fn main() {
    let example = fs::read_to_string("../../../../../examples/core/normalize_add.sexpr")
        .expect("Failed to read file.");

    let nodes = equality_saturate(&example, vec![RuleSet::Core], CostFn::AstSize);

    println!("{:#?}", nodes);
}
