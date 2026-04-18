use crate::RuleSet;
use crate::mim_slotted::MimSlotted;
use crate::mim_slotted::analysis::MimSlottedAnalysis;
use slotted_egraphs::Rewrite;

pub fn get_rules(_rulesets: Vec<RuleSet>) -> Vec<Rewrite<MimSlotted, MimSlottedAnalysis>> {
    Vec::new()
}
