//! `EEActiveGroup`'s plain members as a Rust record the C++ class derives from (the units' state lane, round 1:
//! `class EEActiveGroup : public EEGroup, public EeUnitsActiveGroupFields`, ee/EEPlayer.h). The fields keep their
//! C++ names, types and order (+0x6c `mAction` .. +0xb7 `mIsSpecialAttack` in the original): the action, the 17
//! speeds `ComputeValues` writes as one block (`mSpeed` .. `mCapFactor`), the four flags. The group bodies (ag.rs)
//! read and write them in place through the layout's `active_group_fields` (ag_host.rs).

#![allow(non_snake_case)]

use std::os::raw::c_long;

/// `EEActiveGroup`'s plain members (ee/EEPlayer.h), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsActiveGroupFields {
    /// the command's action (`kActionIdle` 5000 when made; the messages store a goal id)
    pub mAction: c_long,
    /// per stat (the units' max velocity, acceleration, max angular velocity, angular acceleration) the mean, the
    /// maximum and the minimum
    pub mSpeed: f32,
    pub mMaxSpeed: f32,
    pub mMinSpeed: f32,
    pub mAcceleration: f32,
    pub mMaxAcceleration: f32,
    pub mMinAcceleration: f32,
    pub mTurnSpeed: f32,
    pub mMaxTurnSpeed: f32,
    pub mMinTurnSpeed: f32,
    pub mTurnAcceleration: f32,
    pub mMaxTurnAcceleration: f32,
    pub mMinTurnAcceleration: f32,
    /// 1 / the means (0 when not > 0)
    pub mInverseSpeed: f32,
    pub mInverseAcceleration: f32,
    pub mInverseTurnSpeed: f32,
    pub mInverseTurnAcceleration: f32,
    /// the world table's group speed cap factor (the means are capped at the minimum times it)
    pub mCapFactor: f32,
    pub mIsFinished: bool,
    pub mIsQueued: bool,
    pub mMovesTogether: bool,
    pub mIsSpecialAttack: bool,
}

impl EeUnitsActiveGroupFields {
    /// The 17 speeds in their order (`ComputeValues`' block, `mSpeed` .. `mCapFactor`).
    pub fn set_values(&mut self, v: &[f32; 17]) {
        [
            self.mSpeed,
            self.mMaxSpeed,
            self.mMinSpeed,
            self.mAcceleration,
            self.mMaxAcceleration,
            self.mMinAcceleration,
            self.mTurnSpeed,
            self.mMaxTurnSpeed,
            self.mMinTurnSpeed,
            self.mTurnAcceleration,
            self.mMaxTurnAcceleration,
            self.mMinTurnAcceleration,
            self.mInverseSpeed,
            self.mInverseAcceleration,
            self.mInverseTurnSpeed,
            self.mInverseTurnAcceleration,
            self.mCapFactor,
        ] = *v;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem::offset_of;

    #[test]
    fn the_speeds_are_one_block_after_the_action() {
        let long = std::mem::size_of::<c_long>();
        assert_eq!(offset_of!(EeUnitsActiveGroupFields, mSpeed), long);
        assert_eq!(offset_of!(EeUnitsActiveGroupFields, mCapFactor), long + 16 * 4);
        assert_eq!(offset_of!(EeUnitsActiveGroupFields, mIsFinished), long + 17 * 4);
        assert_eq!(offset_of!(EeUnitsActiveGroupFields, mIsSpecialAttack), long + 17 * 4 + 3);
    }

    #[test]
    fn set_values_stores_in_order() {
        let mut f = EeUnitsActiveGroupFields::default();
        let mut v = [0.0f32; 17];
        for (i, x) in v.iter_mut().enumerate() {
            *x = i as f32 + 0.5;
        }
        f.set_values(&v);
        assert_eq!((f.mSpeed, f.mMinTurnSpeed, f.mCapFactor), (0.5, 8.5, 16.5));
    }
}
