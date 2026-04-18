use crate::RuleSet;
use crate::mim_slotted::MimSlotted;
use crate::mim_slotted::analysis::MimSlottedAnalysis;
use slotted_egraphs::Rewrite;

pub fn get_rules(rulesets: Vec<RuleSet>) -> Vec<Rewrite<MimSlotted, MimSlottedAnalysis>> {
    let mut rules = Vec::new();

    for ruleset in rulesets {
        match ruleset {
            _ => (),
        }
    }

    rules
}
