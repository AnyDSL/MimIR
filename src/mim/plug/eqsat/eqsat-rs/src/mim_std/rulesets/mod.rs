use crate::RuleSet;
use crate::mim_std::Mim;
use crate::mim_std::analysis::MimAnalysis;
use egg::Rewrite;

pub mod core;
pub mod math;

pub fn get_rules(rulesets: Vec<RuleSet>) -> Vec<Rewrite<Mim, MimAnalysis>> {
    let mut rules = Vec::new();

    for ruleset in rulesets {
        match ruleset {
            RuleSet::Core => rules.extend(core::rules()),
            RuleSet::Math => rules.extend(math::rules()),
            _ => (),
        }
    }

    rules
}
