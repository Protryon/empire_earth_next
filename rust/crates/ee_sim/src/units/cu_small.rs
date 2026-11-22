//! `EEComplexUnit`'s small bodies (the units' lane, round 9; were EEComplexUnit.cpp): `Clear`, `LastDereference`,
//! `ReleaseSound`, `SetSound`, `ReleaseHealEffect`, `HoldUnit`, `ClearAllGoalQueues`, `PartFinished`, the combat
//! world's ignore and reach wrappers with their CBXR records (`IgnoreTarget`, `IsTargetIgnored`, `CanReachUnit`,
//! `IsOnTileBorderingTarget`) and PayInsurance's chat line. Every C++ statement of the originals is one operation of
//! the unit's host ([`CuHost::cu`], `ee_units_cu_host`: the `EE_UNITS_CU_S_*` below), asked in the original's
//! order; the CBXR records stay C++ statements, each right after the world call it records.

use crate::units::cu_body::CuHost;

// ---- the statements (`op`; the answer). `a` is another object, `b`, `c` values.
/// `mSound != NULL`
pub const EE_UNITS_CU_S_HAS_SOUND: i32 = 4000;
/// `mSound->Dereference()`
pub const EE_UNITS_CU_S_SOUND_DEREFERENCE: i32 = 4001;
/// `mSound = (SESMInstanceNode*)a`
pub const EE_UNITS_CU_S_SET_SOUND: i32 = 4002;
/// `mSound->Reference()`
pub const EE_UNITS_CU_S_SOUND_REFERENCE: i32 = 4003;
/// `UIsNone32(mHealEffect.mValue)`
pub const EE_UNITS_CU_S_HEAL_EFFECT_NONE: i32 = 4004;
/// `gEffectManager->DestroyEffect(mHealEffect)` (a copy of the handle)
pub const EE_UNITS_CU_S_DESTROY_HEAL_EFFECT: i32 = 4005;
/// `mHealEffect.mValue = -1`
pub const EE_UNITS_CU_S_CLEAR_HEAL_EFFECT: i32 = 4006;
/// complex unit `a`'s `Reference()`
pub const EE_UNITS_CU_S_REFERENCE_OTHER: i32 = 4007;
/// complex unit `a`'s `SetHolder(this)`
pub const EE_UNITS_CU_S_SET_HOLDER_OF: i32 = 4008;
/// `mHeldUnits.push_back(a)`
pub const EE_UNITS_CU_S_HELD_PUSH: i32 = 4009;
/// `std::sort(mHeldUnits, EEComplexUnitGrowthLess())`
pub const EE_UNITS_CU_S_HELD_SORT: i32 = 4010;
/// `GetNumGoalQueues()`
pub const EE_UNITS_CU_S_NUM_GOAL_QUEUES: i32 = 4011;
/// `ClearGoalQueue(b)`
pub const EE_UNITS_CU_S_CLEAR_GOAL_QUEUE: i32 = 4012;
/// `SightOn(GetTileX(), GetTileY())`
pub const EE_UNITS_CU_S_SIGHT_ON_HERE: i32 = 4013;
/// `gWorld.GetCurrentPlayer() != mPlayer`
pub const EE_UNITS_CU_S_NOT_CURRENT_PLAYER: i32 = 4014;
/// `gMiniMap->MoveUnit(this, -10000, -10000, GetTileX(), GetTileY())`
pub const EE_UNITS_CU_S_MINIMAP_SHOW: i32 = 4015;
/// `EECombatWorld(EE_COMBAT_WORLD_IGNORE, this, a)`
pub const EE_UNITS_CU_S_WORLD_IGNORE: i32 = 4016;
/// `EECombatCaptureIgnored(EE_COMBAT_XR_IGNORE, this, a, 0, 0)` (TEST ONLY: CBXR)
pub const EE_UNITS_CU_S_CAPTURE_IGNORE: i32 = 4017;
/// `EECombatWorld(b ? EE_COMBAT_WORLD_IS_IGNORED : EE_COMBAT_WORLD_IS_IGNORED_ANYWHERE, this, a) != 0`
pub const EE_UNITS_CU_S_WORLD_IS_IGNORED: i32 = 4018;
/// `EECombatCaptureIgnored(EE_COMBAT_XR_IS_IGNORED, this, a, b, c)` (TEST ONLY: CBXR)
pub const EE_UNITS_CU_S_CAPTURE_IS_IGNORED: i32 = 4019;
/// `EECombatWorld(EE_COMBAT_WORLD_CAN_REACH_UNIT, this, a) != 0`
pub const EE_UNITS_CU_S_WORLD_CAN_REACH: i32 = 4020;
/// `EECombatCaptureXRead(EE_COMBAT_XR_CAN_REACH_UNIT, this, a, b)` (TEST ONLY: CBXR)
pub const EE_UNITS_CU_S_CAPTURE_CAN_REACH: i32 = 4021;
/// `EECombatWorld(EE_COMBAT_WORLD_BORDERING, this, a) != 0`
pub const EE_UNITS_CU_S_WORLD_BORDERING: i32 = 4022;
/// `EECombatCaptureXRead(EE_COMBAT_XR_BORDERING, this, a, b)` (TEST ONLY: CBXR)
pub const EE_UNITS_CU_S_CAPTURE_BORDERING: i32 = 4023;
/// `ResetEEComplexUnit(true, false)`
pub const EE_UNITS_CU_S_RESET_COMPLEX: i32 = 4024;
/// `ResetEEUnit()`
pub const EE_UNITS_CU_S_RESET_UNIT: i32 = 4025;
/// `Reset()`
pub const EE_UNITS_CU_S_RESET: i32 = 4026;
/// `mIsValid = false`
pub const EE_UNITS_CU_S_SET_INVALID: i32 = 4027;
/// `FreeNode()`
pub const EE_UNITS_CU_S_FREE_NODE: i32 = 4028;
/// `mReferenceCount = 0`
pub const EE_UNITS_CU_S_ZERO_REFERENCES: i32 = 4029;
/// `EEComplexUnit::sMemoryPool.Delete(this)`
pub const EE_UNITS_CU_S_POOL_DELETE: i32 = 4030;
/// `sprintf(theAmount, "%ld", b)` (the chat's locals are the host's)
pub const EE_UNITS_CU_S_INSURANCE_AMOUNT: i32 = 4031;
/// `theText = UWideString()`
pub const EE_UNITS_CU_S_INSURANCE_TEXT_NEW: i32 = 4032;
/// `thePart = UWideString()`
pub const EE_UNITS_CU_S_INSURANCE_PART_NEW: i32 = 4033;
/// `gResource->GetString(IDS_YOU_HAVE_RECEIVED, theText)`
pub const EE_UNITS_CU_S_INSURANCE_RECEIVED: i32 = 4034;
/// `theText += theAmount`
pub const EE_UNITS_CU_S_INSURANCE_APPEND_AMOUNT: i32 = 4035;
/// `theText += " "`
pub const EE_UNITS_CU_S_INSURANCE_APPEND_SPACE: i32 = 4036;
/// `gResource->GetString(b + IDS_RESOURCE_NAME_BASE, thePart)`
pub const EE_UNITS_CU_S_INSURANCE_RESOURCE_NAME: i32 = 4037;
/// `theText += thePart`
pub const EE_UNITS_CU_S_INSURANCE_APPEND_PART: i32 = 4038;
/// `gResource->GetString(IDS_IN_INSURANCE, thePart)`
pub const EE_UNITS_CU_S_INSURANCE_IN_INSURANCE: i32 = 4039;
/// `gUserInterface->AddChatLogEntry(player a's mColor, UWideString(gEmptyString), theText, 5000, 2)`
pub const EE_UNITS_CU_S_INSURANCE_CHAT_LINE: i32 = 4040;

fn q<H: CuHost + ?Sized>(h: &mut H, op: i32, unit: u64) -> i64 {
    h.cu(op, unit, 0, 0, 0, &mut [0.0; 4])
}

fn qa<H: CuHost + ?Sized>(h: &mut H, op: i32, unit: u64, a: u64) -> i64 {
    h.cu(op, unit, a, 0, 0, &mut [0.0; 4])
}

fn qab<H: CuHost + ?Sized>(h: &mut H, op: i32, unit: u64, a: u64, b: i64) -> i64 {
    h.cu(op, unit, a, b, 0, &mut [0.0; 4])
}

/// `Clear()`: the complex unit's, the unit's and the base's resets, then invalid.
pub fn clear<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    for op in [EE_UNITS_CU_S_RESET_COMPLEX, EE_UNITS_CU_S_RESET_UNIT, EE_UNITS_CU_S_RESET, EE_UNITS_CU_S_SET_INVALID] {
        q(h, op, unit);
    }
}

/// `LastDereference()`: the node freed and the unit back in the pool.
pub fn last_dereference<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    for op in [EE_UNITS_CU_S_FREE_NODE, EE_UNITS_CU_S_ZERO_REFERENCES, EE_UNITS_CU_S_POOL_DELETE] {
        q(h, op, unit);
    }
}

/// `ReleaseSound()`: the playing sound released.
pub fn release_sound<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    if q(h, EE_UNITS_CU_S_HAS_SOUND, unit) != 0 {
        q(h, EE_UNITS_CU_S_SOUND_DEREFERENCE, unit);
    }
    qa(h, EE_UNITS_CU_S_SET_SOUND, unit, 0);
}

/// `SetSound(sound)`: the old sound released, the new one kept (referenced).
pub fn set_sound<H: CuHost + ?Sized>(h: &mut H, unit: u64, sound: u64) {
    release_sound(h, unit);
    qa(h, EE_UNITS_CU_S_SET_SOUND, unit, sound);
    if sound != 0 {
        q(h, EE_UNITS_CU_S_SOUND_REFERENCE, unit);
    }
}

/// `ReleaseHealEffect()`: the heal effect destroyed and its handle cleared.
pub fn release_heal_effect<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    if q(h, EE_UNITS_CU_S_HEAL_EFFECT_NONE, unit) == 0 {
        q(h, EE_UNITS_CU_S_DESTROY_HEAL_EFFECT, unit);
        q(h, EE_UNITS_CU_S_CLEAR_HEAL_EFFECT, unit);
    }
}

/// `HoldUnit(other)`: the other unit referenced, held by this one and kept in the growth order.
pub fn hold_unit<H: CuHost + ?Sized>(h: &mut H, unit: u64, other: u64) {
    qa(h, EE_UNITS_CU_S_REFERENCE_OTHER, unit, other);
    qa(h, EE_UNITS_CU_S_SET_HOLDER_OF, unit, other);
    qa(h, EE_UNITS_CU_S_HELD_PUSH, unit, other);
    q(h, EE_UNITS_CU_S_HELD_SORT, unit);
}

/// `ClearAllGoalQueues()`.
pub fn clear_all_goal_queues<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let mut i = 0;
    while i < q(h, EE_UNITS_CU_S_NUM_GOAL_QUEUES, unit) {
        qab(h, EE_UNITS_CU_S_CLEAR_GOAL_QUEUE, unit, 0, i);
        i += 1;
    }
}

/// `PartFinished()`: the finished part's sight, and the minimap for another player's.
pub fn part_finished<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    q(h, EE_UNITS_CU_S_SIGHT_ON_HERE, unit);
    if q(h, EE_UNITS_CU_S_NOT_CURRENT_PLAYER, unit) != 0 {
        q(h, EE_UNITS_CU_S_MINIMAP_SHOW, unit);
    }
}

/// `IgnoreTarget(target)`: the combat world's ignore, then its CBXR record.
pub fn ignore_target<H: CuHost + ?Sized>(h: &mut H, unit: u64, target: u64) {
    qa(h, EE_UNITS_CU_S_WORLD_IGNORE, unit, target);
    qa(h, EE_UNITS_CU_S_CAPTURE_IGNORE, unit, target);
}

/// `IsTargetIgnored(target, check location)`: the combat world's answer, recorded.
pub fn is_target_ignored<H: CuHost + ?Sized>(h: &mut H, unit: u64, target: u64, check_location: bool) -> bool {
    let check = i64::from(check_location);
    let result = qab(h, EE_UNITS_CU_S_WORLD_IS_IGNORED, unit, target, check) != 0;
    h.cu(EE_UNITS_CU_S_CAPTURE_IS_IGNORED, unit, target, i64::from(result), check, &mut [0.0; 4]);
    result
}

/// `CanReachUnit(target)` (`bordering` false) or `IsOnTileBorderingTarget(target)`: the combat world's answer,
/// recorded.
pub fn reach<H: CuHost + ?Sized>(h: &mut H, unit: u64, target: u64, bordering: bool) -> bool {
    let (world, capture) = if bordering {
        (EE_UNITS_CU_S_WORLD_BORDERING, EE_UNITS_CU_S_CAPTURE_BORDERING)
    } else {
        (EE_UNITS_CU_S_WORLD_CAN_REACH, EE_UNITS_CU_S_CAPTURE_CAN_REACH)
    };
    let result = qa(h, world, unit, target) != 0;
    qab(h, capture, unit, target, i64::from(result));
    result
}

/// PayInsurance's notice "You have received <amount> <resource> in insurance" for `player`.
pub fn insurance_chat<H: CuHost + ?Sized>(h: &mut H, unit: u64, player: u64, resource: i64, shown: i64) {
    qab(h, EE_UNITS_CU_S_INSURANCE_AMOUNT, unit, 0, shown);
    q(h, EE_UNITS_CU_S_INSURANCE_TEXT_NEW, unit);
    q(h, EE_UNITS_CU_S_INSURANCE_PART_NEW, unit);
    q(h, EE_UNITS_CU_S_INSURANCE_RECEIVED, unit);
    q(h, EE_UNITS_CU_S_INSURANCE_APPEND_AMOUNT, unit);
    q(h, EE_UNITS_CU_S_INSURANCE_APPEND_SPACE, unit);
    qab(h, EE_UNITS_CU_S_INSURANCE_RESOURCE_NAME, unit, 0, resource);
    q(h, EE_UNITS_CU_S_INSURANCE_APPEND_PART, unit);
    q(h, EE_UNITS_CU_S_INSURANCE_IN_INSURANCE, unit);
    q(h, EE_UNITS_CU_S_INSURANCE_APPEND_PART, unit);
    qa(h, EE_UNITS_CU_S_INSURANCE_CHAT_LINE, unit, player);
}

// ---- the bodies (`ee_units_cu_small`'s `what`)
pub const EE_UNITS_CU_S_DO_CLEAR: i32 = 1;
pub const EE_UNITS_CU_S_DO_LAST_DEREFERENCE: i32 = 2;
pub const EE_UNITS_CU_S_DO_RELEASE_SOUND: i32 = 3;
pub const EE_UNITS_CU_S_DO_SET_SOUND: i32 = 4;
pub const EE_UNITS_CU_S_DO_RELEASE_HEAL_EFFECT: i32 = 5;
pub const EE_UNITS_CU_S_DO_HOLD_UNIT: i32 = 6;
pub const EE_UNITS_CU_S_DO_CLEAR_ALL_GOAL_QUEUES: i32 = 7;
pub const EE_UNITS_CU_S_DO_PART_FINISHED: i32 = 8;
pub const EE_UNITS_CU_S_DO_IGNORE_TARGET: i32 = 9;
pub const EE_UNITS_CU_S_DO_IS_TARGET_IGNORED: i32 = 10;
pub const EE_UNITS_CU_S_DO_CAN_REACH_UNIT: i32 = 11;
pub const EE_UNITS_CU_S_DO_IS_ON_TILE_BORDERING: i32 = 12;

/// Runs body `what` on `unit` (`a` another object, `b` a value).
pub fn run<H: CuHost + ?Sized>(h: &mut H, what: i32, unit: u64, a: u64, b: i64) -> i64 {
    match what {
        EE_UNITS_CU_S_DO_CLEAR => clear(h, unit),
        EE_UNITS_CU_S_DO_LAST_DEREFERENCE => last_dereference(h, unit),
        EE_UNITS_CU_S_DO_RELEASE_SOUND => release_sound(h, unit),
        EE_UNITS_CU_S_DO_SET_SOUND => set_sound(h, unit, a),
        EE_UNITS_CU_S_DO_RELEASE_HEAL_EFFECT => release_heal_effect(h, unit),
        EE_UNITS_CU_S_DO_HOLD_UNIT => hold_unit(h, unit, a),
        EE_UNITS_CU_S_DO_CLEAR_ALL_GOAL_QUEUES => clear_all_goal_queues(h, unit),
        EE_UNITS_CU_S_DO_PART_FINISHED => part_finished(h, unit),
        EE_UNITS_CU_S_DO_IGNORE_TARGET => ignore_target(h, unit, a),
        EE_UNITS_CU_S_DO_IS_TARGET_IGNORED => return i64::from(is_target_ignored(h, unit, a, b != 0)),
        EE_UNITS_CU_S_DO_CAN_REACH_UNIT => return i64::from(reach(h, unit, a, false)),
        EE_UNITS_CU_S_DO_IS_ON_TILE_BORDERING => return i64::from(reach(h, unit, a, true)),
        _ => {}
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::cu_body::game::Game;
    use std::os::raw::c_void;

    /// `EEComplexUnit`'s body `what` (`EE_UNITS_CU_S_DO_*`) on `unit`.
    ///
    /// # Safety
    /// `unit` is a live `EEComplexUnit` (`LastDereference` returns it to its pool), `a` live as the body needs it.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_small(what: i32, unit: *mut c_void, a: *mut c_void, b: i64) -> i64 {
        run(&mut Game, what, unit as usize as u64, a as usize as u64, b)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Rec {
        log: Vec<(i32, u64, i64, i64)>,
        answers: Vec<(i32, i64)>,
        queues: i64,
    }

    impl CuHost for Rec {
        fn cu(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64, _f: &mut [f32; 4]) -> i64 {
            assert_eq!(unit, 1);
            self.log.push((op, a, b, c));
            if op == EE_UNITS_CU_S_NUM_GOAL_QUEUES {
                return self.queues;
            }
            self.answers.iter().find(|e| e.0 == op).map_or(0, |e| e.1)
        }
    }

    fn ops(r: &Rec) -> Vec<i32> {
        r.log.iter().map(|e| e.0).collect()
    }

    #[test]
    fn clear_and_last_dereference_in_order() {
        let mut r = Rec::default();
        clear(&mut r, 1);
        assert_eq!(ops(&r), [EE_UNITS_CU_S_RESET_COMPLEX, EE_UNITS_CU_S_RESET_UNIT, EE_UNITS_CU_S_RESET, EE_UNITS_CU_S_SET_INVALID]);
        let mut r = Rec::default();
        last_dereference(&mut r, 1);
        assert_eq!(ops(&r), [EE_UNITS_CU_S_FREE_NODE, EE_UNITS_CU_S_ZERO_REFERENCES, EE_UNITS_CU_S_POOL_DELETE]);
    }

    #[test]
    fn sounds_are_released_before_the_new_one_is_kept() {
        let mut r = Rec { answers: vec![(EE_UNITS_CU_S_HAS_SOUND, 1)], ..Default::default() };
        set_sound(&mut r, 1, 0x50);
        assert_eq!(
            r.log,
            [
                (EE_UNITS_CU_S_HAS_SOUND, 0, 0, 0),
                (EE_UNITS_CU_S_SOUND_DEREFERENCE, 0, 0, 0),
                (EE_UNITS_CU_S_SET_SOUND, 0, 0, 0),
                (EE_UNITS_CU_S_SET_SOUND, 0x50, 0, 0),
                (EE_UNITS_CU_S_SOUND_REFERENCE, 0, 0, 0)
            ]
        );
        let mut r = Rec::default();
        set_sound(&mut r, 1, 0);
        assert_eq!(ops(&r), [EE_UNITS_CU_S_HAS_SOUND, EE_UNITS_CU_S_SET_SOUND, EE_UNITS_CU_S_SET_SOUND]);
    }

    #[test]
    fn the_heal_effect_is_destroyed_only_when_there_is_one() {
        let mut r = Rec::default();
        release_heal_effect(&mut r, 1);
        assert_eq!(ops(&r), [EE_UNITS_CU_S_HEAL_EFFECT_NONE, EE_UNITS_CU_S_DESTROY_HEAL_EFFECT, EE_UNITS_CU_S_CLEAR_HEAL_EFFECT]);
        let mut r = Rec { answers: vec![(EE_UNITS_CU_S_HEAL_EFFECT_NONE, 1)], ..Default::default() };
        release_heal_effect(&mut r, 1);
        assert_eq!(ops(&r), [EE_UNITS_CU_S_HEAL_EFFECT_NONE]);
    }

    #[test]
    fn holding_queues_and_parts() {
        let mut r = Rec::default();
        hold_unit(&mut r, 1, 7);
        assert_eq!(ops(&r), [EE_UNITS_CU_S_REFERENCE_OTHER, EE_UNITS_CU_S_SET_HOLDER_OF, EE_UNITS_CU_S_HELD_PUSH, EE_UNITS_CU_S_HELD_SORT]);
        assert!(r.log[..3].iter().all(|e| e.1 == 7));
        let mut r = Rec { queues: 2, ..Default::default() };
        clear_all_goal_queues(&mut r, 1);
        let cleared: Vec<i64> = r.log.iter().filter(|e| e.0 == EE_UNITS_CU_S_CLEAR_GOAL_QUEUE).map(|e| e.2).collect();
        assert_eq!(cleared, [0, 1]);
        let mut r = Rec { answers: vec![(EE_UNITS_CU_S_NOT_CURRENT_PLAYER, 1)], ..Default::default() };
        part_finished(&mut r, 1);
        assert_eq!(ops(&r), [EE_UNITS_CU_S_SIGHT_ON_HERE, EE_UNITS_CU_S_NOT_CURRENT_PLAYER, EE_UNITS_CU_S_MINIMAP_SHOW]);
    }

    #[test]
    fn the_combat_world_wrappers_record_each_answer_after_it() {
        let mut r = Rec { answers: vec![(EE_UNITS_CU_S_WORLD_IS_IGNORED, 1)], ..Default::default() };
        assert!(is_target_ignored(&mut r, 1, 9, true));
        assert_eq!(r.log, [(EE_UNITS_CU_S_WORLD_IS_IGNORED, 9, 1, 0), (EE_UNITS_CU_S_CAPTURE_IS_IGNORED, 9, 1, 1)]);
        let mut r = Rec::default();
        assert!(!reach(&mut r, 1, 9, false));
        assert_eq!(r.log, [(EE_UNITS_CU_S_WORLD_CAN_REACH, 9, 0, 0), (EE_UNITS_CU_S_CAPTURE_CAN_REACH, 9, 0, 0)]);
        let mut r = Rec { answers: vec![(EE_UNITS_CU_S_WORLD_BORDERING, 1)], ..Default::default() };
        assert!(reach(&mut r, 1, 9, true));
        assert_eq!(r.log[1], (EE_UNITS_CU_S_CAPTURE_BORDERING, 9, 1, 0));
        let mut r = Rec::default();
        ignore_target(&mut r, 1, 9);
        assert_eq!(ops(&r), [EE_UNITS_CU_S_WORLD_IGNORE, EE_UNITS_CU_S_CAPTURE_IGNORE]);
    }

    #[test]
    fn the_insurance_line_is_built_in_the_originals_order() {
        let mut r = Rec::default();
        insurance_chat(&mut r, 1, 0x60, 3, 100);
        assert_eq!(
            ops(&r),
            [
                EE_UNITS_CU_S_INSURANCE_AMOUNT,
                EE_UNITS_CU_S_INSURANCE_TEXT_NEW,
                EE_UNITS_CU_S_INSURANCE_PART_NEW,
                EE_UNITS_CU_S_INSURANCE_RECEIVED,
                EE_UNITS_CU_S_INSURANCE_APPEND_AMOUNT,
                EE_UNITS_CU_S_INSURANCE_APPEND_SPACE,
                EE_UNITS_CU_S_INSURANCE_RESOURCE_NAME,
                EE_UNITS_CU_S_INSURANCE_APPEND_PART,
                EE_UNITS_CU_S_INSURANCE_IN_INSURANCE,
                EE_UNITS_CU_S_INSURANCE_APPEND_PART,
                EE_UNITS_CU_S_INSURANCE_CHAT_LINE
            ]
        );
        assert_eq!(r.log[0].2, 100);
        assert_eq!(r.log[6].2, 3);
        assert_eq!(r.log[10].1, 0x60);
    }
}
