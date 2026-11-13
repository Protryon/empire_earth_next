//! The scenario elements' `Load` and `Save` (scenario files and saved games): each value through the stream in the
//! original's order, the original's version tests and fix-ups, stopping at the first error as its
//! `if (theResult == kErrorOK)` chains did.

use std::os::raw::{c_long, c_ulong};

use crate::fields::*;
use crate::ftol;
use crate::stream::{
    EeScenarioEntry, Seq, Stream, EE_SCENARIO_ENTRIES_CONDITIONS as CONDITIONS, EE_SCENARIO_ENTRIES_EFFECTS as EFFECTS,
};

/// The effect's string slots (`EESMEffect`'s members; the C++ hands them in this order).
pub const EE_SCENARIO_EFFECT_NAME: u32 = 0;
pub const EE_SCENARIO_EFFECT_TEXT: u32 = 1;
pub const EE_SCENARIO_EFFECT_MESSAGE: u32 = 2;
pub const EE_SCENARIO_EFFECT_S64: u32 = 3;
pub const EE_SCENARIO_EFFECT_S78: u32 = 4;
pub const EE_SCENARIO_EFFECT_NEW_NAME: u32 = 5;
pub const EE_SCENARIO_EFFECT_FILE_NAME: u32 = 6;
pub const EE_SCENARIO_EFFECT_DESCRIPTION: u32 = 7;
/// The condition's string slots.
pub const EE_SCENARIO_CONDITION_NAME: u32 = 0;
pub const EE_SCENARIO_CONDITION_TEXT: u32 = 1;
pub const EE_SCENARIO_CONDITION_CHAT_TEXT: u32 = 2;
/// The object's string slot and the id list slot of the effect (`mObjects`) and the object (`mUnits`).
pub const EE_SCENARIO_OBJECT_NAME: u32 = 0;
pub const EE_SCENARIO_LIST_IDS: u32 = 0;

/// `major > 0 || minor >= version` (the saves' tests; `major <= 0` is `major == 0` unsigned).
fn from(major: c_ulong, minor: c_ulong, version: c_ulong) -> bool {
    major > 0 || minor >= version
}

/// `EESMEffect::Load`: a later player action moves down by 2 (then to Victory) whatever the read gave; files before
/// 0.0xcd number the unit attributes after Construction Complete 3 lower; the object list is replaced.
pub fn effect_load<S: Stream>(s: &mut S, f: &mut EeScenarioEffectFields, major: c_ulong, minor: c_ulong) -> i32 {
    let mut q = Seq::new(s);
    q.string(EE_SCENARIO_EFFECT_NAME);
    q.string(EE_SCENARIO_EFFECT_TEXT);
    q.ulong(&mut f.mDuration);
    q.string(EE_SCENARIO_EFFECT_MESSAGE);
    q.string(EE_SCENARIO_EFFECT_S64);
    q.string(EE_SCENARIO_EFFECT_S78);
    q.string(EE_SCENARIO_EFFECT_NEW_NAME);
    q.string(EE_SCENARIO_EFFECT_FILE_NAME);
    q.string(EE_SCENARIO_EFFECT_DESCRIPTION);
    q.ulong(&mut f.mPlayerAttribute);
    q.ulong(&mut f.mDiplomacy);
    q.ulong(&mut f.mMessagePlayer);
    q.ulong(&mut f.mNewOwner);
    q.ulong(&mut f.mPlayer);
    q.ulong(&mut f.mDiplomacyPlayer);
    q.ulong(&mut f.mMediaAction);
    q.ulong(&mut f.mULF4);
    q.ulong(&mut f.mPlayerAction);
    if f.mPlayerAction >= EE_SCENARIO_PLAYER_ACTION_TOTAL {
        f.mPlayerAction -= 2;
    }
    if f.mPlayerAction >= EE_SCENARIO_PLAYER_ACTION_TOTAL {
        f.mPlayerAction = 0;
    }
    q.ulong(&mut f.mTriggerState);
    q.ulong(&mut f.mType);
    q.ulong(&mut f.mObjectAction);
    q.long(&mut f.mTechnology);
    q.ulong(&mut f.mPlacedEffect);
    q.ulong(&mut f.mPlaceArea);
    q.ulong(&mut f.mPlaceObject);
    q.ulong(&mut f.mDaylightHour);
    q.ulong(&mut f.mCycleTime);
    q.ulong(&mut f.mGameAction);
    q.ulong(&mut f.mTextObject);
    q.ulong(&mut f.mAttribute);
    if q.ok() && major == 0 && minor < 0xcd && f.mAttribute > EE_SCENARIO_ATTRIBUTE_CONSTRUCTION_COMPLETE {
        f.mAttribute += 3;
    }
    q.long(&mut f.mAttributePlayer);
    q.ulong(&mut f.mAttributeUnitType);
    q.ulong(&mut f.mAttributeUnit);
    q.ulong(&mut f.mAttributeMode);
    q.ulong(&mut f.mAttributeAmount);
    q.ulong(&mut f.mObject);
    q.ulong(&mut f.mStance);
    q.ulong(&mut f.mTaskAction);
    q.ulong(&mut f.mTaskArea);
    q.ulong(&mut f.mTaskObject);
    q.ulong(&mut f.mCivPoints);
    q.ulong(&mut f.mPlayerAttributeMode);
    q.ulong(&mut f.mPlayerAttributeAmount);
    q.ulong(&mut f.mLocationArea);
    q.ulong(&mut f.mLocationObject);
    q.ulong(&mut f.mTrigger);
    q.bool(&mut f.mB18D);
    q.bool(&mut f.mIsClearText);
    q.bool(&mut f.mUsesLocationArea);
    q.bool(&mut f.mUsesPercent);
    q.bool(&mut f.mIsAddTechnology);
    q.bool(&mut f.mUsesPlaceArea);
    q.bool(&mut f.mB193);
    q.ulong(&mut f.mAIAction);
    q.ulong(&mut f.mTargetArea);
    q.ulong(&mut f.mAIAttribute);
    q.bool(&mut f.mIsOn);
    q.float(&mut f.mFloatValue);
    q.long(&mut f.mIntValue1);
    q.long(&mut f.mIntValue2);
    q.list_replace(EE_SCENARIO_LIST_IDS);
    q.ulong(&mut f.mULD0);
    q.ulong(&mut f.mTargetObject);
    q.ulong(&mut f.mAIToggle);
    q.bool(&mut f.mUsesTargetArea);
    q.bool(&mut f.mIsAddTarget);
    q.ulong(&mut f.mID);
    q.bool(&mut f.mIsAttack);
    q.bool(&mut f.mUsesFavorites);
    q.r
}

/// `EESMEffect::Save`: the civ points from minor version 0x2a, the byte after Uses Place Area from 0x14.
pub fn effect_save<S: Stream>(s: &mut S, f: &EeScenarioEffectFields, major: c_ulong, minor: c_ulong) -> i32 {
    let mut q = Seq::new(s);
    q.put_string(EE_SCENARIO_EFFECT_NAME);
    q.put_string(EE_SCENARIO_EFFECT_TEXT);
    q.put_ulong(f.mDuration);
    q.put_string(EE_SCENARIO_EFFECT_MESSAGE);
    q.put_string(EE_SCENARIO_EFFECT_S64);
    q.put_string(EE_SCENARIO_EFFECT_S78);
    q.put_string(EE_SCENARIO_EFFECT_NEW_NAME);
    q.put_string(EE_SCENARIO_EFFECT_FILE_NAME);
    q.put_string(EE_SCENARIO_EFFECT_DESCRIPTION);
    q.put_ulong(f.mPlayerAttribute);
    q.put_ulong(f.mDiplomacy);
    q.put_ulong(f.mMessagePlayer);
    q.put_ulong(f.mNewOwner);
    q.put_ulong(f.mPlayer);
    q.put_ulong(f.mDiplomacyPlayer);
    q.put_ulong(f.mMediaAction);
    q.put_ulong(f.mULF4);
    q.put_ulong(f.mPlayerAction);
    q.put_ulong(f.mTriggerState);
    q.put_ulong(f.mType);
    q.put_ulong(f.mObjectAction);
    q.put_long(f.mTechnology);
    q.put_ulong(f.mPlacedEffect);
    q.put_ulong(f.mPlaceArea);
    q.put_ulong(f.mPlaceObject);
    q.put_ulong(f.mDaylightHour);
    q.put_ulong(f.mCycleTime);
    q.put_ulong(f.mGameAction);
    q.put_ulong(f.mTextObject);
    q.put_ulong(f.mAttribute);
    q.put_long(f.mAttributePlayer);
    q.put_ulong(f.mAttributeUnitType);
    q.put_ulong(f.mAttributeUnit);
    q.put_ulong(f.mAttributeMode);
    q.put_ulong(f.mAttributeAmount);
    q.put_ulong(f.mObject);
    q.put_ulong(f.mStance);
    q.put_ulong(f.mTaskAction);
    q.put_ulong(f.mTaskArea);
    q.put_ulong(f.mTaskObject);
    if from(major, minor, 0x2a) {
        q.put_ulong(f.mCivPoints);
    }
    q.put_ulong(f.mPlayerAttributeMode);
    q.put_ulong(f.mPlayerAttributeAmount);
    q.put_ulong(f.mLocationArea);
    q.put_ulong(f.mLocationObject);
    q.put_ulong(f.mTrigger);
    q.put_bool(f.mB18D);
    q.put_bool(f.mIsClearText);
    q.put_bool(f.mUsesLocationArea);
    q.put_bool(f.mUsesPercent);
    q.put_bool(f.mIsAddTechnology);
    q.put_bool(f.mUsesPlaceArea);
    if from(major, minor, 0x14) {
        q.put_bool(f.mB193);
    }
    q.put_ulong(f.mAIAction);
    q.put_ulong(f.mTargetArea);
    q.put_ulong(f.mAIAttribute);
    q.put_bool(f.mIsOn);
    q.put_float(f.mFloatValue);
    q.put_long(f.mIntValue1);
    q.put_long(f.mIntValue2);
    q.put_list(EE_SCENARIO_LIST_IDS);
    q.put_ulong(f.mULD0);
    q.put_ulong(f.mTargetObject);
    q.put_ulong(f.mAIToggle);
    q.put_bool(f.mUsesTargetArea);
    q.put_bool(f.mIsAddTarget);
    q.put_ulong(f.mID);
    q.put_bool(f.mIsAttack);
    q.put_bool(f.mUsesFavorites);
    q.r
}

/// `EESMCondition::Load`: the chat text's own result is ignored; a diplomacy above 1 becomes 1 (whatever its read
/// gave).
pub fn condition_load<S: Stream>(s: &mut S, f: &mut EeScenarioConditionFields) -> i32 {
    let mut q = Seq::new(s);
    q.string(EE_SCENARIO_CONDITION_NAME);
    q.string(EE_SCENARIO_CONDITION_TEXT);
    if q.ok() {
        q.s.load_string(EE_SCENARIO_CONDITION_CHAT_TEXT);
        q.ulong(&mut f.mType);
    }
    q.ulong(&mut f.mObjectIndex);
    q.ulong(&mut f.mPlayer);
    q.ulong(&mut f.mAttribute);
    q.ulong(&mut f.mOperator);
    q.ulong(&mut f.mAmount);
    q.ulong(&mut f.mOtherPlayer);
    q.ulong(&mut f.mOtherAttribute);
    q.long(&mut f.mTechnology);
    q.ulong(&mut f.mTrigger);
    q.bool(&mut f.mIsTriggerOn);
    q.bool(&mut f.mHasTriggerFired);
    q.bool(&mut f.mUsesPlayerAttribute);
    q.ulong(&mut f.mMaxTime);
    q.ulong(&mut f.mMinTime);
    q.ulong(&mut f.mHour);
    q.bool(&mut f.mUsesDifficulty);
    q.bool(&mut f.mUsesElapsedTime);
    q.bool(&mut f.mUsesHour);
    q.ulong(&mut f.mDifficulty);
    if q.ok() {
        q.ulong(&mut f.mDiplomacy);
        if f.mDiplomacy >= 2 {
            f.mDiplomacy = 1;
        }
    }
    q.ulong(&mut f.mID);
    q.r
}

/// `EESMCondition::Save`.
pub fn condition_save<S: Stream>(s: &mut S, f: &EeScenarioConditionFields) -> i32 {
    let mut q = Seq::new(s);
    q.put_string(EE_SCENARIO_CONDITION_NAME);
    q.put_string(EE_SCENARIO_CONDITION_TEXT);
    q.put_string(EE_SCENARIO_CONDITION_CHAT_TEXT);
    q.put_ulong(f.mType);
    q.put_ulong(f.mObjectIndex);
    q.put_ulong(f.mPlayer);
    q.put_ulong(f.mAttribute);
    q.put_ulong(f.mOperator);
    q.put_ulong(f.mAmount);
    q.put_ulong(f.mOtherPlayer);
    q.put_ulong(f.mOtherAttribute);
    q.put_long(f.mTechnology);
    q.put_ulong(f.mTrigger);
    q.put_bool(f.mIsTriggerOn);
    q.put_bool(f.mHasTriggerFired);
    q.put_bool(f.mUsesPlayerAttribute);
    q.put_ulong(f.mMaxTime);
    q.put_ulong(f.mMinTime);
    q.put_ulong(f.mHour);
    q.put_bool(f.mUsesDifficulty);
    q.put_bool(f.mUsesElapsedTime);
    q.put_bool(f.mUsesHour);
    q.put_ulong(f.mDifficulty);
    q.put_ulong(f.mDiplomacy);
    q.put_ulong(f.mID);
    q.r
}

/// `EESMObject::Load`: the units are appended (the list is not cleared); the parent from minor version 0x2a, the
/// percent flag from 0x15; files before 0.0xcd number the attributes after Construction Complete 3 lower.
pub fn object_load<S: Stream>(s: &mut S, f: &mut EeScenarioObjectFields, major: c_ulong, minor: c_ulong) -> i32 {
    let mut q = Seq::new(s);
    q.string(EE_SCENARIO_OBJECT_NAME);
    let mut count: c_ulong = 0;
    let mut unit: c_ulong = 0;
    q.ulong(&mut count);
    if q.ok() {
        let mut i: c_ulong = 0;
        while i < count {
            q.r = q.s.read_ulong(&mut unit);
            if !q.ok() {
                break;
            }
            q.s.list_push(EE_SCENARIO_LIST_IDS, unit);
            i += 1;
        }
    }
    q.bool(&mut f.mIsSelectOnMap);
    q.ulong(&mut f.mOwner);
    q.ulong(&mut f.mFamily);
    q.ulong(&mut f.mClass);
    if minor >= 0x2a {
        q.ulong(&mut f.mParent);
    }
    q.ulong(&mut f.mMinCount);
    q.ulong(&mut f.mMaxCount);
    q.bool(&mut f.mIsDynamic);
    q.ulong(&mut f.mInAreaCheck);
    q.ulong(&mut f.mNearCheck);
    q.ulong(&mut f.mHasAttributeCheck);
    q.ulong(&mut f.mVisibleByCheck);
    q.ulong(&mut f.mHasStateCheck);
    q.ulong(&mut f.mSelectedByCheck);
    q.ulong(&mut f.mArea);
    q.ulong(&mut f.mNearMode);
    q.ulong(&mut f.mNearObject);
    q.ulong(&mut f.mAttribute);
    if q.ok() && major == 0 && minor < 0xcd && f.mAttribute > EE_SCENARIO_ATTRIBUTE_CONSTRUCTION_COMPLETE {
        f.mAttribute += 3;
    }
    q.ulong(&mut f.mAttributeMin);
    q.ulong(&mut f.mAttributeMax);
    q.ulong(&mut f.mVisibleByPlayer);
    q.ulong(&mut f.mState);
    q.ulong(&mut f.mSelectedByPlayer);
    if minor >= 0x15 {
        q.bool(&mut f.mIsAttributePercent);
    }
    q.ulong(&mut f.mL64);
    q.r
}

/// `EESMObject::Save` (the parent from minor version 0x2a, the percent flag from 0x15).
pub fn object_save<S: Stream>(s: &mut S, f: &EeScenarioObjectFields, minor: c_ulong) -> i32 {
    let mut q = Seq::new(s);
    q.put_string(EE_SCENARIO_OBJECT_NAME);
    q.put_list(EE_SCENARIO_LIST_IDS);
    q.put_bool(f.mIsSelectOnMap);
    q.put_ulong(f.mOwner);
    q.put_ulong(f.mFamily);
    q.put_ulong(f.mClass);
    if minor >= 0x2a {
        q.put_ulong(f.mParent);
    }
    q.put_ulong(f.mMinCount);
    q.put_ulong(f.mMaxCount);
    q.put_bool(f.mIsDynamic);
    q.put_ulong(f.mInAreaCheck);
    q.put_ulong(f.mNearCheck);
    q.put_ulong(f.mHasAttributeCheck);
    q.put_ulong(f.mVisibleByCheck);
    q.put_ulong(f.mHasStateCheck);
    q.put_ulong(f.mSelectedByCheck);
    q.put_ulong(f.mArea);
    q.put_ulong(f.mNearMode);
    q.put_ulong(f.mNearObject);
    q.put_ulong(f.mAttribute);
    q.put_ulong(f.mAttributeMin);
    q.put_ulong(f.mAttributeMax);
    q.put_ulong(f.mVisibleByPlayer);
    q.put_ulong(f.mState);
    q.put_ulong(f.mSelectedByPlayer);
    if minor >= 0x15 {
        q.put_bool(f.mIsAttributePercent);
    }
    q.put_ulong(f.mL64);
    q.r
}

/// `EESMTrigger::Load`: the settings, then the condition entries and the effect entries (each as `SetX` and
/// `SetXOperator` do: a non-end operator on the last entry appends one); an id of -1 becomes 0. The entries' locals
/// carry over from one entry to the next, as the original's.
pub fn trigger_load<S: Stream>(s: &mut S, f: &mut EeScenarioTriggerFields) -> i32 {
    let mut not = false;
    let mut count: c_ulong = 0;
    let mut op: c_ulong = 0;
    let mut value: c_ulong = 0;
    let mut delay: c_ulong = 0;
    let mut q = Seq::new(s);
    q.string(0);
    q.string(1);
    q.bool(&mut f.mIsOn);
    q.bool(&mut f.mIsLooping);
    q.bool(&mut f.mHasFired);
    q.bool(&mut f.mIsInCinematic);
    q.ulong(&mut f.mConditionDuration);
    q.ulong(&mut f.mConditionStartTime);
    q.ulong(&mut f.mID);
    q.ulong(&mut count);
    if !q.ok() {
        return q.r;
    }
    let mut i: c_ulong = 0;
    while i < count {
        let mut r = q.s.read_bool(&mut not);
        if r == 0 {
            r = q.s.read_ulong(&mut op);
            if r == 0 {
                r = q.s.read_ulong(&mut value);
            }
        }
        if is_none32(value) {
            value = 0;
        }
        if r != 0 {
            return r;
        }
        q.s.entry_set(CONDITIONS, i, EeScenarioEntry { first: not as c_ulong, op: op as c_long, value });
        i += 1;
    }
    let mut r = q.s.read_ulong(&mut count);
    if r == 0 {
        let mut i: c_ulong = 0;
        while i < count {
            r = q.s.read_ulong(&mut delay);
            if r == 0 {
                r = q.s.read_ulong(&mut op);
                if r == 0 {
                    r = q.s.read_ulong(&mut value);
                }
            }
            if is_none32(value) {
                value = 0;
            }
            if r != 0 {
                return r;
            }
            q.s.entry_set(EFFECTS, i, EeScenarioEntry { first: delay, op: op as c_long, value });
            i += 1;
        }
    }
    r
}

/// `EESMTrigger::Save`: the settings, the condition entries (NOT flag, operator, index), the effect entries (delay,
/// operator, index); the effects' count is written only after no error.
pub fn trigger_save<S: Stream>(s: &mut S, f: &EeScenarioTriggerFields) -> i32 {
    let mut q = Seq::new(s);
    q.put_string(0);
    q.put_string(1);
    q.put_bool(f.mIsOn);
    q.put_bool(f.mIsLooping);
    q.put_bool(f.mHasFired);
    q.put_bool(f.mIsInCinematic);
    q.put_ulong(f.mConditionDuration);
    q.put_ulong(f.mConditionStartTime);
    q.put_ulong(f.mID);
    let count = q.s.entry_count(CONDITIONS);
    q.put_ulong(count);
    if q.ok() {
        let mut i: c_ulong = 0;
        while i < q.s.entry_count(CONDITIONS) {
            let e = q.s.entry_at(CONDITIONS, i);
            q.r = q.s.write_bool(e.first != 0);
            if q.r == 0 {
                q.r = q.s.write_ulong(e.op as c_ulong);
            }
            if q.r != 0 {
                break;
            }
            q.r = q.s.write_ulong(e.value);
            if q.r != 0 {
                break;
            }
            i += 1;
        }
    }
    if q.r != 0 {
        return q.r;
    }
    let count = q.s.entry_count(EFFECTS);
    q.r = q.s.write_ulong(count);
    if q.r != 0 {
        return q.r;
    }
    let mut i: c_ulong = 0;
    while i < q.s.entry_count(EFFECTS) {
        let e = q.s.entry_at(EFFECTS, i);
        let mut r = q.s.write_ulong(e.first);
        if r == 0 {
            r = q.s.write_ulong(e.op as c_ulong);
        }
        if r != 0 {
            return r;
        }
        r = q.s.write_ulong(e.value);
        if r != 0 {
            return r;
        }
        q.r = r;
        i += 1;
    }
    q.r
}

/// `EESMArea::Load`: the anchor read as two floats (stored, cut toward zero, only when both read), the rectangle's
/// four floats (stored only when all four read), the flags, the select mode, the tiles' time and the id.
pub fn area_load<S: Stream>(s: &mut S, a: &mut EeScenarioAreaSave) -> i32 {
    let mut q = Seq::new(s);
    q.string(0);
    q.string(1);
    if q.ok() {
        let mut x = a.anchor_x as f32;
        let mut y = 0.0f32;
        q.r = q.s.read_float(&mut x);
        if q.ok() {
            y = a.anchor_y as f32;
            q.r = q.s.read_float(&mut y);
        }
        if q.ok() {
            a.anchor_x = ftol(x) as c_long;
            a.anchor_y = ftol(y) as c_long;
        }
    }
    if q.ok() {
        let (mut left, mut top, mut right, mut bottom) = (0.0f32, 0.0f32, 0.0f32, 0.0f32);
        q.float(&mut left);
        q.float(&mut top);
        q.float(&mut right);
        q.float(&mut bottom);
        if q.ok() {
            (a.left, a.top, a.right, a.bottom) = (left, top, right, bottom);
        }
    }
    q.bool(&mut a.has_rectangle);
    q.bool(&mut a.has_anchor);
    q.ulong(&mut a.select_mode);
    q.ulong(&mut a.tiles_time);
    q.ulong(&mut a.id);
    q.r
}

/// `EESMArea::Save`.
pub fn area_save<S: Stream>(s: &mut S, a: &EeScenarioAreaSave) -> i32 {
    let mut q = Seq::new(s);
    q.put_string(0);
    q.put_string(1);
    q.put_float(a.anchor_x as f32);
    q.put_float(a.anchor_y as f32);
    q.put_float(a.left);
    q.put_float(a.top);
    q.put_float(a.right);
    q.put_float(a.bottom);
    q.put_bool(a.has_rectangle);
    q.put_bool(a.has_anchor);
    q.put_ulong(a.select_mode);
    q.put_ulong(a.tiles_time);
    q.put_ulong(a.id);
    q.r
}

/// `EESMMediaStrings::Load` (`save` false) or `Save`: the seven strings in order, stopping at the first error.
pub fn strings<S: Stream>(s: &mut S, count: u32, save: bool) -> i32 {
    let mut q = Seq::new(s);
    for slot in 0..count {
        if save {
            q.put_string(slot);
        } else {
            q.string(slot);
        }
    }
    q.r
}

#[cfg(test)]
#[allow(clippy::field_reassign_with_default)] // the tests set a few fields of a sample
mod tests {
    use super::*;
    use crate::stream::Bytes;

    fn effect_sample() -> EeScenarioEffectFields {
        let mut f = effect_defaults();
        f.mType = 5;
        f.mPlayerAction = 3;
        f.mTechnology = -2;
        f.mFloatValue = 1.25;
        f.mIntValue1 = -7;
        f.mAttribute = 20;
        f.mID = 0x1234;
        f.mB193 = true;
        f.mUsesFavorites = true;
        f
    }

    #[test]
    fn an_effect_round_trips_with_its_strings_and_list() {
        let f = effect_sample();
        let mut b = Bytes::default();
        b.strings = (0..8).map(|i| format!("s{i}").into_bytes()).collect();
        b.lists = vec![vec![3, 1, 4]];
        assert_eq!(effect_save(&mut b, &f, 0, 0x100), 0);
        // 8 strings of 3+4 bytes, 48 longs and floats, 12 bools, the list's count and 3 ids
        assert_eq!(b.data.len(), 8 * 7 + 48 * 4 + 12 + 4 * 4);
        let mut r = Bytes::over(b.data.clone());
        r.lists = vec![vec![9, 9]];
        let mut g = effect_defaults();
        assert_eq!(effect_load(&mut r, &mut g, 0, 0x100), 0);
        assert_eq!(g, f);
        assert_eq!(r.lists[0], vec![3, 1, 4]);
        assert_eq!(r.strings[7], b"s7");
        assert_eq!(r.at, r.data.len());
    }

    #[test]
    fn effect_versions_and_fix_ups() {
        let f = effect_sample();
        let mut b = Bytes::default();
        b.lists = vec![vec![]];
        // before 0x14 neither the civ points nor the byte after Uses Place Area are written
        assert_eq!(effect_save(&mut b, &f, 0, 0x10), 0);
        let mut full = Bytes::default();
        full.lists = vec![vec![]];
        effect_save(&mut full, &f, 0, 0x100);
        assert_eq!(full.data.len() - b.data.len(), 5);
        // an old file's attribute after Construction Complete moves up 3; the player action 13 down 2
        let mut g = f;
        g.mPlayerAction = 13;
        let mut c = Bytes::default();
        c.lists = vec![vec![]];
        effect_save(&mut c, &g, 1, 0);
        let mut r = Bytes::over(c.data.clone());
        let mut h = effect_defaults();
        assert_eq!(effect_load(&mut r, &mut h, 0, 0xcc), 0);
        assert_eq!((h.mAttribute, h.mPlayerAction), (23, 11));
        let mut r = Bytes::over(c.data.clone());
        let mut h = effect_defaults();
        effect_load(&mut r, &mut h, 1, 0);
        assert_eq!((h.mAttribute, h.mPlayerAction), (20, 11));
    }

    #[test]
    fn a_short_effect_stops_at_its_first_error_but_fixes_the_player_action() {
        let mut g = effect_sample();
        g.mPlayerAction = 50;
        let mut c = Bytes::default();
        c.lists = vec![vec![]];
        effect_save(&mut c, &g, 0, 0x100);
        // cut inside the trigger state, after the player action
        let cut = 8 * 7 + 4 * 10;
        let mut r = Bytes::over(c.data[..cut + 2].to_vec());
        let mut h = effect_defaults();
        assert_eq!(effect_load(&mut r, &mut h, 0, 0x100), 9);
        assert_eq!(h.mPlayerAction, 0);
        assert_eq!(h.mTriggerState, effect_defaults().mTriggerState);
        assert_eq!(h.mID, 0);
    }

    #[test]
    fn a_condition_round_trips_and_clamps_its_diplomacy() {
        let mut f = condition_defaults();
        f.mTechnology = -3;
        f.mDiplomacy = 5;
        f.mUsesHour = true;
        f.mID = 77;
        let mut b = Bytes::default();
        b.strings = vec![b"a".to_vec(), b"bb".to_vec(), b"chat".to_vec()];
        assert_eq!(condition_save(&mut b, &f), 0);
        assert_eq!(b.data.len(), (4 + 2) + (4 + 3) + (4 + 5) + 16 * 4 + 6);
        let mut r = Bytes::over(b.data.clone());
        let mut g = condition_defaults();
        assert_eq!(condition_load(&mut r, &mut g), 0);
        f.mDiplomacy = 1;
        assert_eq!(g, f);
        assert_eq!(r.strings[2], b"chat");
    }

    #[test]
    fn an_object_appends_its_units_and_round_trips_by_version() {
        let mut f = object_defaults();
        f.mParent = 3;
        f.mAttribute = 17;
        f.mIsAttributePercent = true;
        f.mL64 = 9;
        let mut b = Bytes::default();
        b.strings = vec![b"obj".to_vec()];
        b.lists = vec![vec![11, 12]];
        assert_eq!(object_save(&mut b, &f, 0x100), 0);
        let mut r = Bytes::over(b.data.clone());
        r.lists = vec![vec![10]];
        let mut g = object_defaults();
        assert_eq!(object_load(&mut r, &mut g, 1, 0x100), 0);
        assert_eq!(g, f);
        assert_eq!(r.lists[0], vec![10, 11, 12]);
        // before 0x15: no parent, no percent flag, and the attribute moves up 3
        let mut old = Bytes::default();
        old.strings = vec![b"obj".to_vec()];
        old.lists = vec![vec![]];
        object_save(&mut old, &f, 0x14);
        assert_eq!(b.data.len() - old.data.len(), 4 + 1 + 8);
        let mut r = Bytes::over(old.data.clone());
        r.lists = vec![vec![]];
        let mut g = object_defaults();
        assert_eq!(object_load(&mut r, &mut g, 0, 0x14), 0);
        assert_eq!((g.mParent, g.mAttribute, g.mIsAttributePercent), (0, 20, false));
    }

    #[test]
    fn a_trigger_round_trips_its_entries_and_maps_none_to_0() {
        let f = EeScenarioTriggerFields {
            mConditionDuration: 5,
            mConditionStartTime: 9,
            mID: 3,
            mIsOn: true,
            mIsInCinematic: true,
            ..Default::default()
        };
        let mut b = Bytes::default();
        b.strings = vec![b"t".to_vec(), Vec::new()];
        let e = |first, op, value| EeScenarioEntry { first, op, value };
        b.entries = [vec![e(1, 1, 4), e(0, 0, 0xffff_ffff)], vec![e(30, 2, 1), e(0, 0, 2)]];
        assert_eq!(trigger_save(&mut b, &f), 0);
        // 2 strings (6 + 5), 4 bools, 3 numbers, 2 counts, 2 conditions of 9 bytes, 2 effects of 12
        assert_eq!(b.data.len(), 11 + 4 + 12 + 8 + 18 + 24);
        let mut r = Bytes::over(b.data.clone());
        r.entries = [vec![EeScenarioEntry::default()], vec![EeScenarioEntry::default()]];
        let mut g = EeScenarioTriggerFields::default();
        assert_eq!(trigger_load(&mut r, &mut g), 0);
        assert_eq!(g, f);
        assert_eq!(r.entries[0], vec![e(1, 1, 4), e(0, 0, 0)]);
        assert_eq!(r.entries[1], vec![e(30, 2, 1), e(0, 0, 2)]);
        // a cut file returns its error at the entry it stops in
        let mut r = Bytes::over(b.data[..40].to_vec());
        r.entries = [vec![EeScenarioEntry::default()], vec![EeScenarioEntry::default()]];
        assert_eq!(trigger_load(&mut r, &mut EeScenarioTriggerFields::default()), 9);
        assert_eq!(r.entries[0].len(), 2);
    }

    #[test]
    fn an_area_stores_its_anchor_only_when_both_read_and_cuts_toward_zero() {
        let a = EeScenarioAreaSave {
            anchor_x: -3,
            anchor_y: 7,
            left: 1.5,
            top: 2.0,
            right: 9.25,
            bottom: 4.0,
            has_rectangle: true,
            select_mode: 2,
            tiles_time: 11,
            id: 6,
            ..Default::default()
        };
        let mut b = Bytes::default();
        b.strings = vec![b"a".to_vec(), b"b".to_vec()];
        assert_eq!(area_save(&mut b, &a), 0);
        let mut r = Bytes::over(b.data.clone());
        let mut g = EeScenarioAreaSave::default();
        assert_eq!(area_load(&mut r, &mut g), 0);
        assert_eq!(g, a);
        // an anchor of -2.7 is -2; a file cut inside the rectangle keeps the old rectangle
        let mut data = b.data.clone();
        data[12..16].copy_from_slice(&(-2.7f32).to_bits().to_le_bytes());
        let mut r = Bytes::over(data[..26].to_vec());
        let mut g = EeScenarioAreaSave { left: 8.0, ..Default::default() };
        assert_eq!(area_load(&mut r, &mut g), 9);
        assert_eq!((g.anchor_x, g.anchor_y, g.left), (-2, 7, 8.0));
    }

    #[test]
    fn media_strings_stop_at_the_first_error() {
        let mut b = Bytes::default();
        b.strings = (0..7).map(|i| vec![b'a' + i as u8]).collect();
        assert_eq!(strings(&mut b, 7, true), 0);
        let mut r = Bytes::over(b.data[..14].to_vec());
        assert_eq!(strings(&mut r, 7, false), 9);
        assert_eq!(r.strings.len(), 2);
    }
}
