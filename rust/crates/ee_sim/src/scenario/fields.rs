//! The scenario elements' numbers as records the C++ classes derive from (`class EESMEffect : public
//! EeScenarioEffectFields`, ee/EESMEffect.h; the condition's and the object's the same): the fields keep their C++
//! names and types (`unsigned long`, `long`, `float`, `bool`), so every C++ reader is unchanged, and the defaults,
//! saves and index upkeep are these functions. The strings and id lists stay members of the C++ class (the
//! [`crate::stream`] reaches them by slot).

#![allow(non_snake_case)]
// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts to the 32-bit records are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::scenario::condition::EeScenarioCondition;
use crate::scenario::object::EeScenarioObjectFilter;

impl From<&EeScenarioConditionFields> for EeScenarioCondition {
    /// The condition's test record (each field as the C++'s 32-bit cast).
    fn from(c: &EeScenarioConditionFields) -> Self {
        EeScenarioCondition {
            kind: c.mType as u32,
            op: c.mOperator as u32,
            difficulty: c.mDifficulty as u32,
            diplomacy: c.mDiplomacy as u32,
            attribute: c.mAttribute as u32,
            other_attribute: c.mOtherAttribute as u32,
            technology: c.mTechnology as i32,
            amount: c.mAmount as u32,
            object_index: c.mObjectIndex as u32,
            player: c.mPlayer as u32,
            other_player: c.mOtherPlayer as u32,
            min_time: c.mMinTime as u32,
            max_time: c.mMaxTime as u32,
            hour: c.mHour as u32,
            trigger: c.mTrigger as u32,
            is_trigger_on: c.mIsTriggerOn,
            has_trigger_fired: c.mHasTriggerFired,
            uses_difficulty: c.mUsesDifficulty,
            uses_elapsed_time: c.mUsesElapsedTime,
            uses_hour: c.mUsesHour,
            uses_player_attribute: c.mUsesPlayerAttribute,
        }
    }
}

impl From<&EeScenarioObjectFields> for EeScenarioObjectFilter {
    /// The object's class filter (class, family, parent as the C++'s 32-bit casts).
    fn from(o: &EeScenarioObjectFields) -> Self {
        EeScenarioObjectFilter { class: o.mClass as u32, family: o.mFamily as u32, parent: o.mParent as u32 }
    }
}

/// `EESMEffect`'s numbers, in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScenarioEffectFields {
    pub mPlayerAttribute: c_ulong,
    pub mDiplomacy: c_ulong,
    pub mULD0: c_ulong,
    pub mMessagePlayer: c_ulong,
    pub mNewOwner: c_ulong,
    pub mPlayer: c_ulong,
    pub mDiplomacyPlayer: c_ulong,
    pub mAIAction: c_ulong,
    pub mAIAttribute: c_ulong,
    pub mAIToggle: c_ulong,
    pub mMediaAction: c_ulong,
    pub mULF4: c_ulong,
    pub mPlayerAction: c_ulong,
    pub mTriggerState: c_ulong,
    pub mType: c_ulong,
    pub mObjectAction: c_ulong,
    pub mAttributeMode: c_ulong,
    pub mTechnology: c_long,
    pub mFloatValue: f32,
    pub mIntValue1: c_long,
    pub mIntValue2: c_long,
    pub mAttributePlayer: c_long,
    pub mTargetArea: c_ulong,
    pub mTargetObject: c_ulong,
    pub mPlacedEffect: c_ulong,
    pub mPlaceArea: c_ulong,
    pub mPlaceObject: c_ulong,
    pub mDaylightHour: c_ulong,
    pub mCycleTime: c_ulong,
    pub mGameAction: c_ulong,
    pub mDuration: c_ulong,
    pub mTextObject: c_ulong,
    pub mAttribute: c_ulong,
    pub mAttributeAmount: c_ulong,
    pub mAttributeUnitType: c_ulong,
    pub mAttributeUnit: c_ulong,
    pub mObject: c_ulong,
    pub mStance: c_ulong,
    pub mTaskAction: c_ulong,
    pub mTaskArea: c_ulong,
    pub mTaskObject: c_ulong,
    pub mCivPoints: c_ulong,
    pub mPlayerAttributeMode: c_ulong,
    pub mPlayerAttributeAmount: c_ulong,
    pub mLocationArea: c_ulong,
    pub mLocationObject: c_ulong,
    pub mTrigger: c_ulong,
    pub mID: c_ulong,
    pub mIsOn: bool,
    pub mUsesTargetArea: bool,
    pub mIsAttack: bool,
    pub mUsesFavorites: bool,
    pub mIsAddTarget: bool,
    pub mB18D: bool,
    pub mIsClearText: bool,
    pub mUsesLocationArea: bool,
    pub mUsesPercent: bool,
    pub mIsAddTechnology: bool,
    pub mUsesPlaceArea: bool,
    pub mB193: bool,
}

/// `EESMCondition`'s numbers, in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioConditionFields {
    pub mType: c_ulong,
    pub mOperator: c_ulong,
    pub mDifficulty: c_ulong,
    pub mDiplomacy: c_ulong,
    pub mAttribute: c_ulong,
    pub mOtherAttribute: c_ulong,
    pub mTechnology: c_long,
    pub mAmount: c_ulong,
    pub mObjectIndex: c_ulong,
    pub mPlayer: c_ulong,
    pub mOtherPlayer: c_ulong,
    pub mMinTime: c_ulong,
    pub mMaxTime: c_ulong,
    pub mHour: c_ulong,
    pub mTrigger: c_ulong,
    pub mID: c_ulong,
    pub mIsTriggerOn: bool,
    pub mHasTriggerFired: bool,
    pub mUsesDifficulty: bool,
    pub mUsesElapsedTime: bool,
    pub mUsesHour: bool,
    pub mUsesPlayerAttribute: bool,
}

/// `EESMTrigger`'s numbers, in the class's order (its condition and effect entries stay the C++'s).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioTriggerFields {
    pub mConditionDuration: c_ulong,
    pub mConditionStartTime: c_ulong,
    pub mID: c_ulong,
    pub mIsOn: bool,
    pub mHasFired: bool,
    pub mIsLooping: bool,
    pub mIsInCinematic: bool,
}

/// `EESMArea`'s saved values (a copy the C++ fills from its members and stores back): the anchor tile, the
/// rectangle, the flags, the select mode, the tiles' time and the id.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScenarioAreaSave {
    pub anchor_x: c_long,
    pub anchor_y: c_long,
    pub left: f32,
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
    pub has_rectangle: bool,
    pub has_anchor: bool,
    pub select_mode: c_ulong,
    pub tiles_time: c_ulong,
    pub id: c_ulong,
}

/// `EESMObject`'s numbers, in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioObjectFields {
    pub mOwner: c_ulong,
    pub mFamily: c_ulong,
    pub mClass: c_ulong,
    pub mMinCount: c_ulong,
    pub mMaxCount: c_ulong,
    pub mArea: c_ulong,
    pub mNearMode: c_ulong,
    pub mNearObject: c_ulong,
    pub mParent: c_ulong,
    pub mAttribute: c_ulong,
    pub mAttributeMin: c_ulong,
    pub mAttributeMax: c_ulong,
    pub mVisibleByPlayer: c_ulong,
    pub mState: c_ulong,
    pub mSelectedByPlayer: c_ulong,
    pub mL64: c_ulong,
    pub mInAreaCheck: c_ulong,
    pub mNearCheck: c_ulong,
    pub mHasAttributeCheck: c_ulong,
    pub mVisibleByCheck: c_ulong,
    pub mHasStateCheck: c_ulong,
    pub mSelectedByCheck: c_ulong,
    pub mIsSelectOnMap: bool,
    pub mIsDynamic: bool,
    pub mIsAttributePercent: bool,
}

/// `kSMEffectObject`, `kSMEffectNone` (ee/EEScenarioIDs.h).
pub const EE_SCENARIO_EFFECT_NONE: c_ulong = 0;
pub const EE_SCENARIO_EFFECT_OBJECT: c_ulong = 1;
/// `kSMPlayerActionTotal`: Load moves a later player action down by 2, then to Victory (0).
pub const EE_SCENARIO_PLAYER_ACTION_TOTAL: c_ulong = 12;
/// `kSMUnitAttributeConstructionComplete`: files before 0.0xcd number the attributes after it 3 lower.
pub const EE_SCENARIO_ATTRIBUTE_CONSTRUCTION_COMPLETE: c_ulong = 16;
/// `(unsigned long)-1` as C++ stores it in memory (all bits set on every data model).
pub const EE_SCENARIO_NONE: c_ulong = c_ulong::MAX;

/// `EESMEffect::Reset`'s numbers: an object Create effect, 20 s, player 7, the defaults of each action.
pub fn effect_defaults() -> EeScenarioEffectFields {
    EeScenarioEffectFields {
        mDuration: 0x14,
        mMessagePlayer: 7,
        mNewOwner: 7,
        mPlayer: 7,
        mDiplomacyPlayer: 8,
        mType: EE_SCENARIO_EFFECT_OBJECT,
        mDaylightHour: 0xf,
        mAttributePlayer: -1,
        mAttributeUnit: EE_SCENARIO_NONE,
        mCivPoints: 1,
        mIsClearText: true,
        mUsesLocationArea: true,
        mIsAddTechnology: true,
        mUsesPlaceArea: true,
        mULD0: 8,
        mIsAddTarget: true,
        ..Default::default()
    }
}

/// `EESMCondition`'s constructor's numbers: a player-attribute condition, players 7 and 8, attribute 2, operator 2,
/// difficulty 1.
pub fn condition_defaults() -> EeScenarioConditionFields {
    EeScenarioConditionFields {
        mType: 2,
        mPlayer: 7,
        mAttribute: 2,
        mOperator: 2,
        mOtherPlayer: 8,
        mOtherAttribute: 2,
        mDifficulty: 1,
        ..Default::default()
    }
}

/// `EESMObject`'s constructor's numbers: picked on the map, owner and selecting player 7, one unit, dynamic.
pub fn object_defaults() -> EeScenarioObjectFields {
    EeScenarioObjectFields {
        mIsSelectOnMap: true,
        mOwner: 7,
        mSelectedByPlayer: 7,
        mMinCount: 1,
        mMaxCount: 1,
        mIsDynamic: true,
        ..Default::default()
    }
}

/// `UIsNone32`: all 32 low bits set.
#[inline]
pub fn is_none32(value: c_ulong) -> bool {
    value as u32 == u32::MAX
}

/// One index's upkeep after entries `old` and `new` swap, or after `old` is deleted (`new` none): the reference to
/// `old` follows it (to 0 on a delete), one to `new` goes to `old`, and on a delete a later one moves down.
pub fn renumber(index: &mut c_ulong, old: c_ulong, new: c_ulong) {
    let value = *index;
    if value == old {
        *index = if !is_none32(new) { new } else { 0 };
    } else if value == new {
        *index = old;
    } else if is_none32(new) && value > old {
        *index = value - 1;
    }
}

/// What a renumbering follows (`EESMEffect::RenumberAreas`, `RenumberObjects`, `RenumberTriggers`; the condition's
/// and the object's).
pub const EE_SCENARIO_RENUMBER_AREAS: u32 = 0;
pub const EE_SCENARIO_RENUMBER_OBJECTS: u32 = 1;
pub const EE_SCENARIO_RENUMBER_TRIGGERS: u32 = 2;

/// The effect's references of a kind, in the original's order (the object list's entries after these: the C++
/// passes them to [`renumber`] one by one).
pub fn effect_renumber(f: &mut EeScenarioEffectFields, kind: u32, old: c_ulong, new: c_ulong) {
    match kind {
        EE_SCENARIO_RENUMBER_AREAS => {
            renumber(&mut f.mTaskArea, old, new);
            renumber(&mut f.mTargetArea, old, new);
            renumber(&mut f.mPlaceArea, old, new);
            renumber(&mut f.mLocationArea, old, new);
        }
        EE_SCENARIO_RENUMBER_OBJECTS => {
            renumber(&mut f.mObject, old, new);
            renumber(&mut f.mLocationObject, old, new);
            renumber(&mut f.mTaskObject, old, new);
            renumber(&mut f.mPlaceObject, old, new);
            renumber(&mut f.mTargetObject, old, new);
        }
        EE_SCENARIO_RENUMBER_TRIGGERS => renumber(&mut f.mTrigger, old, new),
        _ => {}
    }
}

/// The condition's references of a kind (objects: its object index; triggers: its trigger).
pub fn condition_renumber(f: &mut EeScenarioConditionFields, kind: u32, old: c_ulong, new: c_ulong) {
    match kind {
        EE_SCENARIO_RENUMBER_OBJECTS => renumber(&mut f.mObjectIndex, old, new),
        EE_SCENARIO_RENUMBER_TRIGGERS => renumber(&mut f.mTrigger, old, new),
        _ => {}
    }
}

/// The object's references of a kind (areas: its area; objects: its parent, then its near object).
pub fn object_renumber(f: &mut EeScenarioObjectFields, kind: u32, old: c_ulong, new: c_ulong) {
    match kind {
        EE_SCENARIO_RENUMBER_AREAS => renumber(&mut f.mArea, old, new),
        EE_SCENARIO_RENUMBER_OBJECTS => {
            renumber(&mut f.mParent, old, new);
            renumber(&mut f.mNearObject, old, new);
        }
        _ => {}
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn renumber_follows_a_swap_and_a_delete() {
        let none = EE_SCENARIO_NONE;
        let mut i: c_ulong = 3;
        renumber(&mut i, 3, 5);
        assert_eq!(i, 5);
        renumber(&mut i, 3, 5);
        assert_eq!(i, 3);
        renumber(&mut i, 3, none);
        assert_eq!(i, 0);
        let mut j: c_ulong = 7;
        renumber(&mut j, 3, none);
        assert_eq!(j, 6);
        renumber(&mut j, 9, none);
        assert_eq!(j, 6);
        // a none read from a file (32 bits) is none too
        let mut k: c_ulong = 4;
        renumber(&mut k, 2, 0xffff_ffff as c_ulong);
        assert_eq!(k, 3);
        // a swap leaves an unrelated index
        renumber(&mut k, 1, 2);
        assert_eq!(k, 3);
    }

    #[test]
    fn the_defaults_are_the_constructors() {
        let e = effect_defaults();
        assert_eq!((e.mDuration, e.mType, e.mDaylightHour, e.mULD0, e.mCivPoints), (20, 1, 15, 8, 1));
        assert_eq!((e.mAttributePlayer, is_none32(e.mAttributeUnit), e.mPlayer, e.mDiplomacyPlayer), (-1, true, 7, 8));
        assert!(e.mIsAddTarget && e.mIsClearText && e.mUsesLocationArea && e.mIsAddTechnology && e.mUsesPlaceArea);
        assert!(!e.mIsOn && !e.mB18D && !e.mB193 && !e.mUsesPercent && e.mFloatValue == 0.0);
        let c = condition_defaults();
        assert_eq!((c.mType, c.mPlayer, c.mOtherPlayer, c.mAttribute, c.mOperator, c.mDifficulty), (2, 7, 8, 2, 2, 1));
        let o = object_defaults();
        assert_eq!((o.mOwner, o.mSelectedByPlayer, o.mMinCount, o.mMaxCount), (7, 7, 1, 1));
        assert!(o.mIsSelectOnMap && o.mIsDynamic && !o.mIsAttributePercent);
    }

    #[test]
    fn the_effect_renumbers_its_references_by_kind() {
        let mut f =
            EeScenarioEffectFields { mTaskArea: 4, mPlaceArea: 2, mObject: 4, mTrigger: 4, ..Default::default() };
        effect_renumber(&mut f, EE_SCENARIO_RENUMBER_AREAS, 2, EE_SCENARIO_NONE);
        assert_eq!((f.mTaskArea, f.mPlaceArea, f.mObject, f.mTrigger), (3, 0, 4, 4));
        effect_renumber(&mut f, EE_SCENARIO_RENUMBER_TRIGGERS, 4, 1);
        assert_eq!(f.mTrigger, 1);
        let mut o = EeScenarioObjectFields { mParent: 1, mNearObject: 2, mArea: 2, ..Default::default() };
        object_renumber(&mut o, EE_SCENARIO_RENUMBER_OBJECTS, 1, 2);
        assert_eq!((o.mParent, o.mNearObject, o.mArea), (2, 1, 2));
        let mut c = EeScenarioConditionFields { mObjectIndex: 5, mTrigger: 5, ..Default::default() };
        condition_renumber(&mut c, EE_SCENARIO_RENUMBER_OBJECTS, 0, EE_SCENARIO_NONE);
        assert_eq!((c.mObjectIndex, c.mTrigger), (4, 5));
    }
}
