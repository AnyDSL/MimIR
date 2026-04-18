use crate::mim_slotted::MimSlotted;
use slotted_egraphs::*;

#[derive(Default, Clone)]
pub struct MimSlottedAnalysis;

impl Analysis<MimSlotted> for MimSlottedAnalysis {
    type Data = ();

    fn make(_eg: &EGraph<MimSlotted, Self>, _enode: &MimSlotted) -> Self::Data {}

    fn merge(_l: Self::Data, _r: Self::Data) -> Self::Data {}

    fn modify(_eg: &mut EGraph<MimSlotted, Self>, _id: Id) {}
}
