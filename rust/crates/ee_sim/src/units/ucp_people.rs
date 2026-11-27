//! The people's unit bodies over their C++ statements (the units' lane, round 7; were EEUnitClassPeople.cpp):
//! `EEPlayUnitEffect`, `UpdateCitizen`, the priest's `BasicPriestUpdateConversion`, `BasicPriestConvertTarget`,
//! `EECanConvert` (the decision ee_combat's can_convert, its CBCN record a C++ statement),
//! `BasicPriestShowConversionEffects`/`BasicPriestHideConversionEffects`, and the unit's step timers
//! (`UpdateReloadTimer`, `UpdateTargetSearchTimer`, `UpdateHealSearchTimer`, `UpdateRottingAwayTime`,
//! `UpdateResourcesRotting`: the countdowns step.rs's over the store, their USTM notes notes.rs's, as the C++ called
//! them). Each C++ statement one `EE_UNITS_UCP_*` case of `ee_units_ucp_host` (EEUnitClassPeople.cpp; the class
//! host's statements, `EE_UNITS_UC_*`, reach EEUnitClass.cpp's through it), in the original's order.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::combat::decide::{can_convert as decide_can_convert, EeCombatCanConvertIn, EeCombatCanConvertOut};
use crate::combat::{EeCombatConversionIn, EeCombatConversionOut};
use crate::units::uc_anim::*;
use crate::units::uc_body::*;
use crate::units::uc_create::*;
use crate::units::uc_produce::*;
use std::os::raw::c_long;

// ---- the statements (from 1400; `unit` the citizen or priest)
/// `unit->mGoalQueues[0].mGoals.size()`
pub const EE_UNITS_UCP_QUEUE0_LEN: i32 = 1400;
/// `unit->mGoalQueues[0].mGoals[b]->mKind` (an unsigned char)
pub const EE_UNITS_UCP_QUEUE0_KIND: i32 = 1401;
/// `unit->SetResourceAmount(0)`
pub const EE_UNITS_UCP_CLEAR_RESOURCE_AMOUNT: i32 = 1402;
/// `unit->mPlayer->mTechnologies->Research(b, unit)`
pub const EE_UNITS_UCP_RESEARCH: i32 = 1403;
/// the epoch's removal sound (`EESoundObject::Get(the epoch media's mRemovalSound)`)
pub const EE_UNITS_UCP_REMOVAL_SOUND: i32 = 1404;
/// the epoch media's `mRemovalEffects.size()`
pub const EE_UNITS_UCP_REMOVAL_EFFECT_COUNT: i32 = 1405;
/// `gWorld.GetEffectManager()->CreatePlacedEffect(GE3DPoint(f[0..3]), EEGraphicEffect::Get(the epoch media's
/// mRemovalEffects[b]), &the handle)`
pub const EE_UNITS_UCP_REMOVAL_EFFECT: i32 = 1406;
/// `EEComputeSoundParameters(1.0f, GE3DPoint(f[0..3]), f[0], f[1], f[2], kUnitEffectSoundFalloff)`
pub const EE_UNITS_UCP_SOUND_AT: i32 = 1407;
/// `f[0] = ` the epoch media's `mRemovalPitchOffset`
pub const EE_UNITS_UCP_REMOVAL_PITCH_OFFSET: i32 = 1408;
/// sound `a`'s `Play(f[0], f[1], f[2])`
pub const EE_UNITS_UCP_SOUND_PLAY: i32 = 1409;
/// the epoch media's `mRemovalThunderEvery`
pub const EE_UNITS_UCP_THUNDER_EVERY: i32 = 1410;
/// `EESoundObject::Get(the epoch media's mRemovalThunder)`
pub const EE_UNITS_UCP_THUNDER_SOUND: i32 = 1411;
/// the handle's reset (`theHandle = {0}`)
pub const EE_UNITS_UCP_RESET_HANDLE: i32 = 1412;
/// `unit->mWorkTimer`
pub const EE_UNITS_UCP_WORK_TIMER: i32 = 1413;
/// `unit->SetWorkTimer(b)`
pub const EE_UNITS_UCP_SET_WORK_TIMER: i32 = 1414;
/// `unit->mGoalState->Work(unit)`
pub const EE_UNITS_UCP_GOAL_WORK: i32 = 1415;
/// `unit->UpdateFanaticismColor()`
pub const EE_UNITS_UCP_UPDATE_FANATICISM: i32 = 1416;
/// `unit->mCurrentResearch`
pub const EE_UNITS_UCP_CURRENT_RESEARCH: i32 = 1417;
/// `unit->mPlayer->mTechnologies->Get(unit->mCurrentResearch)`
pub const EE_UNITS_UCP_RESEARCH_NODE: i32 = 1418;
/// node `a`'s `mData->mMakesUnit`
pub const EE_UNITS_UCP_NODE_MAKES_UNIT: i32 = 1419;
/// node `a`'s `mData->mKind`
pub const EE_UNITS_UCP_NODE_KIND: i32 = 1420;
/// `gCivPowersManager.GetPower(kCivPowerBundeswehr, the power)`
pub const EE_UNITS_UCP_GET_BUNDESWEHR: i32 = 1421;
/// the power's `mTechnology == ` node `a`'s `mData->mPrerequisite1`
pub const EE_UNITS_UCP_POWER_IS_PREREQUISITE: i32 = 1422;
/// `unit->GetPower()`
pub const EE_UNITS_UCP_POWER: i32 = 1423;
/// `EEGetConversionCost(complex unit a)`
pub const EE_UNITS_UCP_CONVERSION_COST: i32 = 1424;
/// `static_cast<EEComplexUnit*>(EEAttackUnitCast(unit->GetGoal())->GetAttackTarget())`
pub const EE_UNITS_UCP_CONVERSION_TARGET: i32 = 1425;
/// complex unit `a`'s `mPlayer == unit->mPlayer`
pub const EE_UNITS_UCP_SAME_PLAYER: i32 = 1426;
/// `unit->mPlayer->mEnemies[complex unit a->mPlayer->GetPlayerID()]`
pub const EE_UNITS_UCP_ENEMY_STATE: i32 = 1427;
/// `unit->ReleaseConversionEffect()`
pub const EE_UNITS_UCP_RELEASE_CONVERSION: i32 = 1428;
/// `unit->ClearGoals(0)`
pub const EE_UNITS_UCP_CLEAR_GOALS: i32 = 1429;
/// `UIsNone32(unit->mConversionEffect.mValue)`
pub const EE_UNITS_UCP_NO_CONVERSION_EFFECT: i32 = 1430;
/// `gWorld.GetEffectManager()->CreateUnitEffect(complex unit a,
/// EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator), &the handle, 0, 0, 0)`
pub const EE_UNITS_UCP_CREATE_INDICATOR: i32 = 1431;
/// `unit->mConversionEffect != the handle`
pub const EE_UNITS_UCP_HANDLE_DIFFERS: i32 = 1432;
/// `unit->mConversionEffect = the handle`
pub const EE_UNITS_UCP_KEEP_HANDLE: i32 = 1433;
/// complex unit `a`'s `mPlayer->mEvents.AddPlayerEvent(a's tile, unit's tile, 1, a->GetPlayer(), -1, false)`
pub const EE_UNITS_UCP_CONVERSION_EVENT: i32 = 1434;
/// `gPhysics->mGameTime - unit->mConversionStartTime < (unsigned long)class->mMinConversionTime`
pub const EE_UNITS_UCP_TOO_SOON: i32 = 1435;
/// `unit->mWorkTimer` (an int)
pub const EE_UNITS_UCP_WORK_TIMER_INT: i32 = 1436;
/// `f[0] = ` complex unit `a`'s `mStats->mConversionResistance`
pub const EE_UNITS_UCP_RESISTANCE: i32 = 1437;
/// `gCivPowersManager.HasPower(unit->GetPlayer(), kCivPowerAdaptation)`
pub const EE_UNITS_UCP_HAS_ADAPTATION: i32 = 1438;
/// `unit->mUnitClass->IsCrusader()`
pub const EE_UNITS_UCP_IS_CRUSADER: i32 = 1439;
/// `unit->mUnitClass->mCategory`
pub const EE_UNITS_UCP_UNIT_CATEGORY: i32 = 1440;
/// complex unit `a`'s `mUnitClass->mUnitType`
pub const EE_UNITS_UCP_TYPE_OF: i32 = 1441;
/// `f[0] = unit->mStats->mConversionRate`
pub const EE_UNITS_UCP_CONVERSION_RATE: i32 = 1442;
/// `f[0] = ` complex unit `a`'s `mPlayer->mConversionFactor`
pub const EE_UNITS_UCP_CONVERSION_FACTOR: i32 = 1443;
/// the conversion at `b` = `EECombatConversion(the record at a)` (its CB record)
pub const EE_UNITS_UCP_CONVERSION: i32 = 1444;
/// `f[0] = gPhysics->mRandom.Generate()`
pub const EE_UNITS_UCP_RANDOM: i32 = 1445;
/// complex unit `a`'s `GetComplexUnit()->mIsDisabled`
pub const EE_UNITS_UCP_DISABLED_OF: i32 = 1446;
/// complex unit `a`'s `GetComplexUnit()->IsProtected()`
pub const EE_UNITS_UCP_PROTECTED_OF: i32 = 1447;
/// complex unit `a`'s `mPlayer`
pub const EE_UNITS_UCP_PLAYER_OF: i32 = 1448;
/// `EEConvertUnit(complex unit a, unit->mPlayer->GetPlayerID(), 1)`
pub const EE_UNITS_UCP_CONVERT_UNIT: i32 = 1449;
/// complex unit `a`'s `mStats`
pub const EE_UNITS_UCP_STATS_OF: i32 = 1450;
/// `unit->mPlayer->mAreaConversion == true`
pub const EE_UNITS_UCP_AREA_CONVERSION: i32 = 1451;
/// `ee_units_uu_units_near(f[0..3], gWorldTable.GetFloatValue(kWorldAreaConversionRadius), &the units list)`
pub const EE_UNITS_UCP_UNITS_NEAR: i32 = 1452;
/// the units list's `[b]`
pub const EE_UNITS_UCP_UNITS_AT: i32 = 1453;
/// `((EEMapTile*)unit a->mTile)->IsSeenBy(unit->GetPlayer()) == true`
pub const EE_UNITS_UCP_TILE_SEEN: i32 = 1454;
/// `unit->SetPower(unit->mPower - EEGetConversionCost(unit))`
pub const EE_UNITS_UCP_SPEND_FAITH: i32 = 1455;
/// `unit->mConversionStartTime`
pub const EE_UNITS_UCP_CONVERSION_START: i32 = 1456;
/// `unit->mConversionStartTime = 0`
pub const EE_UNITS_UCP_CLEAR_CONVERSION_START: i32 = 1457;
/// stats `a`'s `mCreationSound`
pub const EE_UNITS_UCP_STATS_CREATION_SOUND: i32 = 1458;
/// `unit->mPlayer == gWorld.mPlayers[gWorld.mCurrentPlayer]`
pub const EE_UNITS_UCP_UNIT_PLAYER_CURRENT: i32 = 1459;
/// player `a == gWorld.mPlayers[gWorld.mCurrentPlayer]`
pub const EE_UNITS_UCP_PLAYER_CURRENT: i32 = 1460;
/// `gSoundManager->Play(sound a->mSampleID, 1.0f, 0.0f, 1.0f, false, NULL)`
pub const EE_UNITS_UCP_PLAY_FULL: i32 = 1461;
/// `class->mCategory`
pub const EE_UNITS_UCP_CLASS_CATEGORY: i32 = 1462;
/// `unit->mCloaked`
pub const EE_UNITS_UCP_CLOAKED: i32 = 1463;
/// `gWorld.GetEffectManager()->CreateUnitEffect(unit, EEGraphicEffect::Get(b), &the handle, 0, 0, 0)`
pub const EE_UNITS_UCP_CREATE_EFFECT: i32 = 1464;
/// `unit->mCloaked = the handle.mValue`
pub const EE_UNITS_UCP_CLOAKED_HANDLE: i32 = 1465;
/// `unit->SetGrowTime(the handle.mValue)`
pub const EE_UNITS_UCP_GROW_HANDLE: i32 = 1466;
/// `EEComputeSoundParameters(EESoundObject::Get(kSoundPriestTowerConvert)->mVolume, GE3DPoint(f[0..3]), f[0], f[1],
/// f[2], kBasicPriestSoundFactor)`
pub const EE_UNITS_UCP_TOWER_SOUND_AT: i32 = 1467;
/// `gSoundManager->Play(EESoundObject::Get(kSoundPriestTowerConvert)->mSampleID, f[0], f[1], f[2], false, NULL)`
pub const EE_UNITS_UCP_PLAY_TOWER_SOUND: i32 = 1468;
/// `(long)unit->mCloaked > 0`
pub const EE_UNITS_UCP_HAS_CLOAKED_EFFECT: i32 = 1469;
/// `gWorld.GetEffectManager()->DestroyEffect(EEUcpHandle(unit->mCloaked))`
pub const EE_UNITS_UCP_DESTROY_CLOAKED_EFFECT: i32 = 1470;
/// `unit->mCloaked = 0`
pub const EE_UNITS_UCP_CLEAR_CLOAKED: i32 = 1471;
/// `unit->mGrowTime > 0`
pub const EE_UNITS_UCP_HAS_GROW_EFFECT: i32 = 1472;
/// `gWorld.GetEffectManager()->DestroyEffect(EEUcpHandle(unit->mGrowTime))`
pub const EE_UNITS_UCP_DESTROY_GROW_EFFECT: i32 = 1473;
/// `unit->SetGrowTime(0)`
pub const EE_UNITS_UCP_CLEAR_GROW_TIME: i32 = 1474;
/// the can-convert reads of priest `unit` and target `a` (`EE_UNITS_UCP_CC_*` in `b`; floats in `f[0]`)
pub const EE_UNITS_UCP_CAN_CONVERT_READ: i32 = 1475;
/// `EECombatCapture(EE_COMBAT_TAG('C', 'B', 'C', 'N'), the record at a, the outcome at b)`
pub const EE_UNITS_UCP_CAN_CONVERT_CAPTURE: i32 = 1476;
/// `unit->mReloadTimer`, `mTargetSearchTimer`, `mRottingAwayTime`, `mResourceAmount` by `b` (0..3); `f[0] =
/// mHealSearchTimer` for 4
pub const EE_UNITS_UCP_TIMER: i32 = 1477;
/// `SetReloadTimer(c)`, `SetTargetSearchTimer(c)`, `SetRottingAwayTime(c)`, `SetResourceAmount(c)` by `b`;
/// `SetHealSearchTimer(f[0])` for 4
pub const EE_UNITS_UCP_SET_TIMER: i32 = 1478;
/// `EESimCaptureRecord::Active()`
pub const EE_UNITS_UCP_CAPTURE_ACTIVE: i32 = 1479;
/// `f[0] = gPhysics->mTickTime`
pub const EE_UNITS_UCP_TICK_TIME: i32 = 1480;
/// the units list's `clear()`
pub const EE_UNITS_UCP_UNITS_CLEAR: i32 = 1481;
/// the units list's `size()`
pub const EE_UNITS_UCP_UNITS_LEN: i32 = 1482;

// the can-convert reads (`EE_UNITS_UCP_CAN_CONVERT_READ`'s `b`)
pub const EE_UNITS_UCP_CC_CRUSADER: i64 = 0;
pub const EE_UNITS_UCP_CC_BUILDING_PRIEST: i64 = 1;
pub const EE_UNITS_UCP_CC_ADAPTATION: i64 = 2;
pub const EE_UNITS_UCP_CC_SAME_PLAYER: i64 = 3;
pub const EE_UNITS_UCP_CC_ENEMY: i64 = 4;
pub const EE_UNITS_UCP_CC_CONVERSION_RATE: i64 = 5;
pub const EE_UNITS_UCP_CC_TARGET_DISABLED: i64 = 6;
pub const EE_UNITS_UCP_CC_TARGET_PROTECTED: i64 = 7;
pub const EE_UNITS_UCP_CC_RESISTANCE: i64 = 8;
pub const EE_UNITS_UCP_CC_TARGET_TYPE: i64 = 9;
pub const EE_UNITS_UCP_CC_TARGET_HIT_POINTS: i64 = 10;
pub const EE_UNITS_UCP_CC_ATTACKABLE: i64 = 11;
pub const EE_UNITS_UCP_CC_AMMO_TYPE: i64 = 12;
pub const EE_UNITS_UCP_CC_DAMAGE: i64 = 13;
pub const EE_UNITS_UCP_CC_TARGET_CATEGORY: i64 = 14;
pub const EE_UNITS_UCP_CC_PRIEST_CATEGORY: i64 = 15;
pub const EE_UNITS_UCP_CC_CONVERTS_PRIESTS: i64 = 16;
pub const EE_UNITS_UCP_CC_POWER: i64 = 17;
pub const EE_UNITS_UCP_CC_CONVERTS_BUILDINGS: i64 = 18;
pub const EE_UNITS_UCP_CC_TARGET_LAND_BUILDING: i64 = 19;
pub const EE_UNITS_UCP_CC_EPOCH: i64 = 20;
pub const EE_UNITS_UCP_CC_RANGE: i64 = 21;
pub const EE_UNITS_UCP_CC_PRIEST_CYBER: i64 = 22;
pub const EE_UNITS_UCP_CC_TARGET_CYBER: i64 = 23;
pub const EE_UNITS_UCP_CC_SHIELDED: i64 = 24;

// ---- the bodies (`ee_units_ucp_call`'s `what`)
pub const EE_UNITS_UCP_DO_PLAY_UNIT_EFFECT: i32 = 1;
pub const EE_UNITS_UCP_DO_UPDATE_CITIZEN: i32 = 2;
pub const EE_UNITS_UCP_DO_UPDATE_CONVERSION: i32 = 3;
pub const EE_UNITS_UCP_DO_CONVERT_TARGET: i32 = 4;
pub const EE_UNITS_UCP_DO_CAN_CONVERT: i32 = 5;
pub const EE_UNITS_UCP_DO_SHOW_CONVERSION: i32 = 6;
pub const EE_UNITS_UCP_DO_HIDE_CONVERSION: i32 = 7;
pub const EE_UNITS_UCP_DO_RELOAD_TIMER: i32 = 8;
pub const EE_UNITS_UCP_DO_TARGET_SEARCH_TIMER: i32 = 9;
pub const EE_UNITS_UCP_DO_HEAL_SEARCH_TIMER: i32 = 10;
pub const EE_UNITS_UCP_DO_ROTTING_TIME: i32 = 11;
pub const EE_UNITS_UCP_DO_RESOURCES_ROTTING: i32 = 12;

const GOAL_ATTACK_UNIT: i64 = 2;
const GOAL_LOAD_TRANSPORT: i64 = 7;
const GOAL_EXPLORE: i64 = 10;
const ACTION_IDLE: i64 = 5000;
const ACTION_REPAIR_BUILDING: i64 = 4002;
const ACTION_GATHER_RESOURCE: i64 = 4004;
const TECH_NONE: i64 = 100000;
const CATEGORY_BUILDING_PRIEST: i64 = 0x1e;
const CATEGORY_LAND_PRIEST: i64 = 0xd;
const UNIT_TYPE_TOWN_CENTER: i64 = 0x23;
const UNIT_TYPE_SETTLEMENT: i64 = 0x2b;
const TECH_EPOCH_COPPER: i64 = 2;
/// the temple's swirl and smoke effects (`kGraphicEffectPriestTowerSwirlEffect`, `...SmokeGenerator`)
const PRIEST_TOWER_EFFECTS: [i64; 2] = [0xd9, 0xda];
/// the step notes' kinds (`kUnitStep*`, ee/EEUnitStore.h)
const STEP_KIND_RELOAD: u32 = 1;
const STEP_KIND_TARGET_SEARCH: u32 = 2;
const STEP_KIND_HEAL_SEARCH: u32 = 3;
const STEP_KIND_ROTTING: u32 = 5;
const STEP_KIND_RESOURCES_ROTTING: u32 = 6;
/// the timers by `EE_UNITS_UCP_TIMER`'s `b`
const TIMER_RELOAD: i64 = 0;
const TIMER_TARGET_SEARCH: i64 = 1;
const TIMER_ROTTING: i64 = 2;
const TIMER_RESOURCE_AMOUNT: i64 = 3;
const TIMER_HEAL_SEARCH: i64 = 4;

/// The step countdowns and notes these bodies call (step.rs's and notes.rs's FFI functions, as the C++ called them).
pub trait UcpRust {
    fn countdown(&mut self, id: u32, object: u64, which: u32, member: i64, tick_ms: i64) -> Option<i64>;
    fn heal_step(&mut self, id: u32, object: u64, member: f32, tick_time: f32) -> Option<f32>;
    fn rotting(&mut self, id: u32, object: u64, member: i64, tick_ms: i64) -> i64;
    fn resources_rotting(&mut self, id: u32, object: u64, time: i64, amount: i64, tick_ms: i64) -> (i64, Option<i64>);
    fn note_step(&mut self, kind: u32, id: u32, words: &[u32]);
}

/// `EEPlayUnitEffect(unit, technology)`: the technology researched on the unit (idling it unless only attack, load
/// or explore goals are queued); if that kills it, the epoch's removal effects and thunder where it stood.
pub fn play_unit_effect<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, technology: i64) {
    let mut s = S { h, class, unit };
    s.s(EE_UNITS_UCP_RESET_HANDLE);
    let mut stop = false;
    if s.s(EE_UNITS_UC_HIT_POINTS) <= 0 {
        return;
    }
    let mut i = s.s(EE_UNITS_UCP_QUEUE0_LEN) as c_long as i64 - 1;
    while i >= 0 && !stop {
        let kind = s.b(EE_UNITS_UCP_QUEUE0_KIND, i) & 0xff;
        if kind == 0 || (kind > GOAL_ATTACK_UNIT && kind != GOAL_LOAD_TRANSPORT && kind != GOAL_EXPLORE) {
            stop = true;
        }
        i -= 1;
    }
    s.s(EE_UNITS_UCP_CLEAR_RESOURCE_AMOUNT);
    if stop {
        s.b(EE_UNITS_UC_START_ACTION_SELF, ACTION_IDLE);
        s.s(EE_UNITS_UC_ACTION_CHANGED_SELF);
    }
    s.s(EE_UNITS_UC_PLAY_NO_EFFECT);
    s.b(EE_UNITS_UC_SET_PRODUCTION_TIME, 0);
    s.b(EE_UNITS_UCP_RESEARCH, technology);
    if s.s(EE_UNITS_UC_HIT_POINTS) > 0 {
        return;
    }
    let mut position = [0.0f32; 3];
    s.point(EE_UNITS_UC_POSITION, unit, 0, 0, &mut position);
    let sound = s.s(EE_UNITS_UCP_REMOVAL_SOUND) as u64;
    for k in 0..s.s(EE_UNITS_UCP_REMOVAL_EFFECT_COUNT) {
        let mut p = position;
        s.point(EE_UNITS_UCP_REMOVAL_EFFECT, 0, k, 0, &mut p);
    }
    if sound == 0 {
        return;
    }
    let mut f = [position[0], position[1], position[2], 0.0];
    s.f(EE_UNITS_UCP_SOUND_AT, 0, 0, 0, &mut f);
    let offset = s.float(EE_UNITS_UCP_REMOVAL_PITCH_OFFSET, 0, 0);
    if offset != 0.0 {
        let pitch = f[2] + offset;
        f[2] = if -1.0 < pitch { pitch } else { -1.0 };
    }
    s.f(EE_UNITS_UCP_SOUND_PLAY, sound, 0, 0, &mut f);
    let every = s.s(EE_UNITS_UCP_THUNDER_EVERY) as u32;
    if every == 0 || !(s.s(EE_UNITS_UC_UNIT_ID) as u32).is_multiple_of(every) {
        return;
    }
    let thunder = s.s(EE_UNITS_UCP_THUNDER_SOUND) as u64;
    if thunder == 0 {
        return;
    }
    let mut f = [position[0], position[1], position[2], 0.0];
    s.f(EE_UNITS_UCP_SOUND_AT, 0, 0, 0, &mut f);
    s.f(EE_UNITS_UCP_SOUND_PLAY, thunder, 0, 0, &mut f);
}

/// `UpdateCitizen(unit)`: the work timer (the gather or repair goal worked when it runs out), then a due unit
/// research finished (at once under instant build); a Bundeswehr technology goes through `EEPlayUnitEffect`.
pub fn update_citizen<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h: &mut *h, class, unit };
    let mut timer = s.s(EE_UNITS_UCP_WORK_TIMER) as c_long;
    let mut step: c_long = 0;
    if timer > 0 {
        step = s.s(EE_UNITS_UC_TICK_MS) as c_long;
        if step == 0 {
            step = 1;
        }
        step = timer.wrapping_sub(step);
        s.b(EE_UNITS_UCP_SET_WORK_TIMER, if step > 0 { step as i64 } else { 0 });
    }
    if s.s(EE_UNITS_UCP_WORK_TIMER) == 0 {
        let action = s.s(EE_UNITS_UC_COMMAND_KIND);
        if action == ACTION_REPAIR_BUILDING || action == ACTION_GATHER_RESOURCE {
            if s.s(EE_UNITS_UC_GOAL_STATE_SELF) != 0 {
                s.s(EE_UNITS_UCP_GOAL_WORK);
            }
            timer = s.s(EE_UNITS_UCP_WORK_TIMER) as c_long;
            if timer > 0 {
                let work = s.s(EE_UNITS_UCP_WORK_TIMER) as c_long;
                let value = if work > step.wrapping_neg() { work.wrapping_add(step) } else { 1 };
                s.b(EE_UNITS_UCP_SET_WORK_TIMER, value as i64);
            }
        }
    }
    s.s(EE_UNITS_UCP_UPDATE_FANATICISM);
    if s.s(EE_UNITS_UCP_CURRENT_RESEARCH) == TECH_NONE {
        return;
    }
    let mut frame = s.s(EE_UNITS_UC_TICK_MS) as c_long;
    if frame == 0 {
        frame = 1;
    }
    let left = (s.s(EE_UNITS_UC_PRODUCTION_TIME) as c_long).wrapping_sub(frame);
    // under instant build a due research finishes at once (the original's goto)
    if s.s(EE_UNITS_UC_INSTANT_BUILD) == 0 && left > 0 {
        return;
    }
    s.b(EE_UNITS_UC_SET_PRODUCTION_TIME, 0);
    let node = s.s(EE_UNITS_UCP_RESEARCH_NODE) as u64;
    if node != 0 && s.a(EE_UNITS_UCP_NODE_MAKES_UNIT, node) == 0 {
        let technology = s.s(EE_UNITS_UCP_CURRENT_RESEARCH);
        let mut research = true;
        if s.a(EE_UNITS_UCP_NODE_KIND, node) == 3
            && s.s(EE_UNITS_UCP_GET_BUNDESWEHR) != 0
            && s.a(EE_UNITS_UCP_POWER_IS_PREREQUISITE, node) != 0
        {
            play_unit_effect(s.h, class, unit, technology);
            research = false;
        }
        s.s(EE_UNITS_UC_PLAY_NO_EFFECT);
        s.b(EE_UNITS_UC_SET_PRODUCTION_TIME, 0);
        if research {
            s.b(EE_UNITS_UCP_RESEARCH, technology);
        }
    }
}

/// `BasicPriestUpdateConversion(unit)`: a conversion worked with enough faith (stopped at an ally or own unit; the
/// effects shown and the owner warned; after the minimum time a try a second, converting on success).
pub fn update_conversion<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h: &mut *h, class, unit };
    if s.s(EE_UNITS_UCP_POWER) < s.a(EE_UNITS_UCP_CONVERSION_COST, unit) {
        return;
    }
    let target = s.s(EE_UNITS_UCP_CONVERSION_TARGET) as u64;
    if s.a(EE_UNITS_UCP_SAME_PLAYER, target) != 0 || s.a(EE_UNITS_UCP_ENEMY_STATE, target) == 0 {
        s.s(EE_UNITS_UCP_RELEASE_CONVERSION);
        s.s(EE_UNITS_UCP_CLEAR_GOALS);
        hide_conversion(s.h, class, unit);
        if s.s(EE_UNITS_UC_GOAL_STATE_SELF) == 0 {
            s.b(EE_UNITS_UC_START_ACTION_SELF, ACTION_IDLE);
        }
        s.s(EE_UNITS_UC_CLASS_ACTION_CHANGED);
        return;
    }
    if s.s(EE_UNITS_UCP_NO_CONVERSION_EFFECT) != 0 {
        s.s(EE_UNITS_UCP_RESET_HANDLE);
        if s.a(EE_UNITS_UCP_CREATE_INDICATOR, target) == 0 && s.s(EE_UNITS_UCP_HANDLE_DIFFERS) != 0 {
            s.s(EE_UNITS_UCP_KEEP_HANDLE);
        }
        show_conversion(s.h, class, unit);
        s.a(EE_UNITS_UCP_CONVERSION_EVENT, target);
    }
    if s.s(EE_UNITS_UC_WORLD_IN_CINEMATIC) != 0 {
        return;
    }
    if s.s(EE_UNITS_UCP_TOO_SOON) != 0 {
        return;
    }
    let mut i = EeCombatConversionIn {
        work_timer: s.s(EE_UNITS_UCP_WORK_TIMER_INT) as i32,
        tick_seconds: s.float(EE_UNITS_UC_TICK_SECONDS, 0, 0),
        resistance: s.float(EE_UNITS_UCP_RESISTANCE, target, 0),
        ..Default::default()
    };
    i.adaptation = i32::from(
        i.resistance == 1.0
            && s.s(EE_UNITS_UCP_HAS_ADAPTATION) != 0
            && s.s(EE_UNITS_UCP_IS_CRUSADER) == 0
            && s.s(EE_UNITS_UCP_UNIT_CATEGORY) != CATEGORY_BUILDING_PRIEST
            && s.a(EE_UNITS_UCP_TYPE_OF, target) == UNIT_TYPE_TOWN_CENTER,
    );
    i.rate = s.float(EE_UNITS_UCP_CONVERSION_RATE, 0, 0);
    i.factor = s.float(EE_UNITS_UCP_CONVERSION_FACTOR, target, 0);
    let mut o = EeCombatConversionOut::default();
    s.ab(EE_UNITS_UCP_CONVERSION, addr(&mut i), addr(&mut o) as i64);
    let time = o.timer;
    if o.due != 0 {
        if s.float(EE_UNITS_UCP_RANDOM, 0, 0) <= o.threshold
            && (s.a(EE_UNITS_UC_TILE_NODE, target) == 0
                || s.a(EE_UNITS_UCP_DISABLED_OF, target) == 0
                || s.a(EE_UNITS_UCP_PROTECTED_OF, target) != 0)
        {
            convert_target(s.h, class, unit, target);
        } else {
            s.b(EE_UNITS_UCP_SET_WORK_TIMER, i64::from(o.miss_timer));
        }
    } else {
        s.b(EE_UNITS_UCP_SET_WORK_TIMER, i64::from(time));
    }
}

/// `BasicPriestConvertTarget(unit, target)`: the target converted to the priest's player (with area conversion,
/// every convertible seen unit nearby too), the faith spent, the creation sound for the two players involved.
pub fn convert_target<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, target: u64) {
    let mut s = S { h: &mut *h, class, unit };
    let old_player = s.a(EE_UNITS_UCP_PLAYER_OF, target) as u64;
    s.s(EE_UNITS_UCP_RELEASE_CONVERSION);
    hide_conversion(s.h, class, unit);
    let converted = s.a(EE_UNITS_UCP_CONVERT_UNIT, target) as u64;
    let stats = s.a(EE_UNITS_UCP_STATS_OF, converted) as u64;
    if s.s(EE_UNITS_UCP_AREA_CONVERSION) != 0 {
        let mut location = [0.0f32; 3];
        s.point(EE_UNITS_UC_LOCATION, converted, 0, 0, &mut location);
        s.s(EE_UNITS_UCP_UNITS_CLEAR);
        s.point(EE_UNITS_UCP_UNITS_NEAR, 0, 0, 0, &mut location);
        let count = s.s(EE_UNITS_UCP_UNITS_LEN);
        let near: Vec<u64> = (0..count).map(|k| s.b(EE_UNITS_UCP_UNITS_AT, k) as u64).collect();
        for &other in near.iter().rev() {
            if s.a(EE_UNITS_UC_COMPLEX_OF, other) != 0 && s.a(EE_UNITS_UCP_TILE_SEEN, other) != 0 {
                let complex = s.a(EE_UNITS_UC_COMPLEX_OF, other) as u64;
                if can_convert(s.h, unit, complex).0 {
                    s.a(EE_UNITS_UCP_CONVERT_UNIT, other);
                }
            }
        }
    }
    s.s(EE_UNITS_UCP_SPEND_FAITH);
    if s.s(EE_UNITS_UCP_CONVERSION_START) != 0 {
        s.s(EE_UNITS_UCP_CLEAR_CONVERSION_START);
    }
    let sound = s.a(EE_UNITS_UCP_STATS_CREATION_SOUND, stats) as u64;
    if sound == 0 {
        return;
    }
    if s.s(EE_UNITS_UCP_UNIT_PLAYER_CURRENT) != 0 || s.a(EE_UNITS_UCP_PLAYER_CURRENT, old_player) != 0 {
        s.a(EE_UNITS_UCP_PLAY_FULL, sound);
    }
}

/// `EECanConvert(priest, target, flag)`: whether the priest can convert the target now (ee_combat's can_convert
/// over the reads; CBCN records it); the flag when the decision writes it.
pub fn can_convert<H: UcHost + ?Sized>(h: &mut H, priest: u64, target: u64) -> (bool, Option<bool>) {
    let mut s = S { h, class: 0, unit: priest };
    let cost = s.a(EE_UNITS_UCP_CONVERSION_COST, priest);
    let r = |s: &mut S<'_, H>, which: i64| s.ab(EE_UNITS_UCP_CAN_CONVERT_READ, target, which);
    let mut i = EeCombatCanConvertIn { crusader: r(&mut s, EE_UNITS_UCP_CC_CRUSADER) as i32, ..Default::default() };
    i.building_priest = i32::from(r(&mut s, EE_UNITS_UCP_CC_PRIEST_CATEGORY) == CATEGORY_BUILDING_PRIEST);
    i.adaptation = r(&mut s, EE_UNITS_UCP_CC_ADAPTATION) as i32;
    i.same_player = r(&mut s, EE_UNITS_UCP_CC_SAME_PLAYER) as i32;
    i.enemy = r(&mut s, EE_UNITS_UCP_CC_ENEMY) as i32;
    let fr = |s: &mut S<'_, H>, which: i64| {
        let mut f = [0.0f32; 4];
        s.f(EE_UNITS_UCP_CAN_CONVERT_READ, target, which, 0, &mut f);
        f[0]
    };
    i.conversion_rate = fr(&mut s, EE_UNITS_UCP_CC_CONVERSION_RATE);
    i.target_disabled = r(&mut s, EE_UNITS_UCP_CC_TARGET_DISABLED) as i32;
    i.target_protected = r(&mut s, EE_UNITS_UCP_CC_TARGET_PROTECTED) as i32;
    i.resistance = fr(&mut s, EE_UNITS_UCP_CC_RESISTANCE);
    i.target_town_center = i32::from(r(&mut s, EE_UNITS_UCP_CC_TARGET_TYPE) == UNIT_TYPE_TOWN_CENTER);
    i.target_settlement = i32::from(r(&mut s, EE_UNITS_UCP_CC_TARGET_TYPE) == UNIT_TYPE_SETTLEMENT);
    i.target_hit_points = r(&mut s, EE_UNITS_UCP_CC_TARGET_HIT_POINTS) as i32;
    i.attackable = r(&mut s, EE_UNITS_UCP_CC_ATTACKABLE) as i32;
    i.damage = if r(&mut s, EE_UNITS_UCP_CC_AMMO_TYPE) >= 0 { fr(&mut s, EE_UNITS_UCP_CC_DAMAGE) } else { 0.0 };
    i.same_category =
        i32::from(r(&mut s, EE_UNITS_UCP_CC_TARGET_CATEGORY) == r(&mut s, EE_UNITS_UCP_CC_PRIEST_CATEGORY));
    i.converts_priests = r(&mut s, EE_UNITS_UCP_CC_CONVERTS_PRIESTS) as i32;
    i.power = r(&mut s, EE_UNITS_UCP_CC_POWER) as i32;
    i.cost = cost as i32;
    i.target_land_priest = i32::from(r(&mut s, EE_UNITS_UCP_CC_TARGET_CATEGORY) == CATEGORY_LAND_PRIEST);
    i.converts_buildings = r(&mut s, EE_UNITS_UCP_CC_CONVERTS_BUILDINGS) as i32;
    i.target_land_building = r(&mut s, EE_UNITS_UCP_CC_TARGET_LAND_BUILDING) as i32;
    i.at_copper = i32::from(r(&mut s, EE_UNITS_UCP_CC_EPOCH) >= TECH_EPOCH_COPPER);
    let mut priest_at = [0.0f32; 3];
    s.point(EE_UNITS_UC_LOCATION, priest, 0, 0, &mut priest_at);
    let mut target_at = [0.0f32; 3];
    s.point(EE_UNITS_UC_LOCATION, target, 0, 0, &mut target_at);
    [i.priest_x, i.priest_y, i.priest_z] = priest_at;
    [i.target_x, i.target_y, i.target_z] = target_at;
    i.range = fr(&mut s, EE_UNITS_UCP_CC_RANGE);
    i.priest_cyber = r(&mut s, EE_UNITS_UCP_CC_PRIEST_CYBER) as i32;
    i.target_cyber = r(&mut s, EE_UNITS_UCP_CC_TARGET_CYBER) as i32;
    i.shielded = r(&mut s, EE_UNITS_UCP_CC_SHIELDED) as i32;
    let mut o: EeCombatCanConvertOut = decide_can_convert(&i);
    let flag = (o.flag_written != 0).then_some(o.flag != 0);
    s.ab(EE_UNITS_UCP_CAN_CONVERT_CAPTURE, addr(&mut i), addr(&mut o) as i64);
    (o.result != 0, flag)
}

/// `BasicPriestShowConversionEffects(unit)`: a temple not already showing them starts the tower swirl and smoke
/// effects and plays the tower convert sound.
pub fn show_conversion<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    if s.s(EE_UNITS_UCP_CLASS_CATEGORY) != CATEGORY_BUILDING_PRIEST || s.s(EE_UNITS_UCP_CLOAKED) != 0 {
        return;
    }
    s.s(EE_UNITS_UCP_RESET_HANDLE);
    if s.b(EE_UNITS_UCP_CREATE_EFFECT, PRIEST_TOWER_EFFECTS[0]) == 0 {
        s.s(EE_UNITS_UCP_CLOAKED_HANDLE);
    }
    if s.b(EE_UNITS_UCP_CREATE_EFFECT, PRIEST_TOWER_EFFECTS[1]) == 0 {
        s.s(EE_UNITS_UCP_GROW_HANDLE);
    }
    let mut position = [0.0f32; 3];
    s.point(EE_UNITS_UC_POSITION, unit, 0, 0, &mut position);
    let mut f = [position[0], position[1], position[2], 0.0];
    s.f(EE_UNITS_UCP_TOWER_SOUND_AT, 0, 0, 0, &mut f);
    s.f(EE_UNITS_UCP_PLAY_TOWER_SOUND, 0, 0, 0, &mut f);
}

/// `BasicPriestHideConversionEffects(unit)`: a temple's tower swirl and smoke effects destroyed.
pub fn hide_conversion<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    if s.s(EE_UNITS_UCP_CLASS_CATEGORY) != CATEGORY_BUILDING_PRIEST {
        return;
    }
    if s.s(EE_UNITS_UCP_HAS_CLOAKED_EFFECT) != 0 {
        s.s(EE_UNITS_UCP_DESTROY_CLOAKED_EFFECT);
        s.s(EE_UNITS_UCP_CLEAR_CLOAKED);
    }
    if s.s(EE_UNITS_UCP_HAS_GROW_EFFECT) == 0 {
        return;
    }
    s.s(EE_UNITS_UCP_DESTROY_GROW_EFFECT);
    s.s(EE_UNITS_UCP_CLEAR_GROW_TIME);
}

/// A step's USTM note when the capture runs (`words`' last the member after the step, read here)
fn note<H: UcHost + UcpRust + ?Sized>(h: &mut H, unit: u64, kind: u32, id: u32, words: &mut Vec<u32>, last: i64) {
    let mut s = S { h: &mut *h, class: 0, unit };
    if s.s(EE_UNITS_UCP_CAPTURE_ACTIVE) == 0 {
        return;
    }
    words.push(if last == TIMER_HEAL_SEARCH {
        s.float(EE_UNITS_UCP_TIMER, 0, TIMER_HEAL_SEARCH).to_bits()
    } else {
        s.b(EE_UNITS_UCP_TIMER, last) as u32
    });
    h.note_step(kind, id, words);
}

/// `UpdateReloadTimer()` (`target_search`: `UpdateTargetSearchTimer()`): the timer counted down by the tick, not
/// below 0 (step.rs over the store), written through the holder; noted as USTM while the capture runs.
pub fn countdown_timer<H: UcHost + UcpRust + ?Sized>(h: &mut H, unit: u64, target_search: bool) {
    let mut s = S { h: &mut *h, class: 0, unit };
    let (which, timer, kind) = if target_search {
        (crate::units::step::EE_UNITS_STEP_TARGET_SEARCH, TIMER_TARGET_SEARCH, STEP_KIND_TARGET_SEARCH)
    } else {
        (crate::units::step::EE_UNITS_STEP_RELOAD, TIMER_RELOAD, STEP_KIND_RELOAD)
    };
    let before = s.b(EE_UNITS_UCP_TIMER, timer) as c_long;
    let id = s.s(EE_UNITS_UC_UNIT_ID) as u32;
    let tick = s.s(EE_UNITS_UC_TICK_MS) as c_long;
    let written = h.countdown(id, unit, which, i64::from(before), i64::from(tick));
    let mut s = S { h: &mut *h, class: 0, unit };
    if let Some(time) = written {
        s.abc(EE_UNITS_UCP_SET_TIMER, 0, timer, time as c_long as i64);
    }
    let tick = s.s(EE_UNITS_UC_TICK_MS) as u32;
    let mut words = vec![before as u32, tick, u32::from(written.is_some())];
    note(h, unit, kind, id, &mut words, timer);
}

/// `UpdateHealSearchTimer()`: the heal search timer (seconds) counted down by the tick, not below 0 (step.rs);
/// noted as USTM while the capture runs.
pub fn heal_search_timer<H: UcHost + UcpRust + ?Sized>(h: &mut H, unit: u64) {
    let mut s = S { h: &mut *h, class: 0, unit };
    let before = s.float(EE_UNITS_UCP_TIMER, 0, TIMER_HEAL_SEARCH);
    let id = s.s(EE_UNITS_UC_UNIT_ID) as u32;
    let tick = s.float(EE_UNITS_UCP_TICK_TIME, 0, 0);
    let written = h.heal_step(id, unit, before, tick);
    let mut s = S { h: &mut *h, class: 0, unit };
    if let Some(time) = written {
        let mut f = [time, 0.0, 0.0, 0.0];
        s.f(EE_UNITS_UCP_SET_TIMER, 0, TIMER_HEAL_SEARCH, 0, &mut f);
    }
    let tick = s.float(EE_UNITS_UCP_TICK_TIME, 0, 0);
    let mut words = vec![before.to_bits(), tick.to_bits(), u32::from(written.is_some())];
    note(h, unit, STEP_KIND_HEAL_SEARCH, id, &mut words, TIMER_HEAL_SEARCH);
}

/// `UpdateRottingAwayTime()`: the time left minus the tick, not below 0 (step.rs), written through the holder; the
/// time.
pub fn rotting_time<H: UcHost + UcpRust + ?Sized>(h: &mut H, unit: u64) -> i64 {
    let mut s = S { h: &mut *h, class: 0, unit };
    let before = s.b(EE_UNITS_UCP_TIMER, TIMER_ROTTING) as c_long;
    let id = s.s(EE_UNITS_UC_UNIT_ID) as u32;
    let tick = s.s(EE_UNITS_UC_TICK_MS) as c_long;
    let time = h.rotting(id, unit, i64::from(before), i64::from(tick)) as c_long;
    let mut s = S { h: &mut *h, class: 0, unit };
    s.abc(EE_UNITS_UCP_SET_TIMER, 0, TIMER_ROTTING, i64::from(time));
    let tick = s.s(EE_UNITS_UC_TICK_MS) as u32;
    let mut words = vec![before as u32, tick];
    note(h, unit, STEP_KIND_ROTTING, id, &mut words, TIMER_ROTTING);
    i64::from(time)
}

/// `UpdateResourcesRotting()`: the time plus the tick; past 4 s the resource amount loses one per whole 4 s
/// (step.rs); noted as USTM while the capture runs.
pub fn resources_rotting<H: UcHost + UcpRust + ?Sized>(h: &mut H, unit: u64) {
    let mut s = S { h: &mut *h, class: 0, unit };
    let before = s.b(EE_UNITS_UCP_TIMER, TIMER_ROTTING) as c_long;
    let amount_before = s.b(EE_UNITS_UCP_TIMER, TIMER_RESOURCE_AMOUNT) as c_long;
    let id = s.s(EE_UNITS_UC_UNIT_ID) as u32;
    let tick = s.s(EE_UNITS_UC_TICK_MS) as c_long;
    let (time, amount) = h.resources_rotting(id, unit, i64::from(before), i64::from(amount_before), i64::from(tick));
    let mut s = S { h: &mut *h, class: 0, unit };
    if let Some(amount) = amount {
        s.abc(EE_UNITS_UCP_SET_TIMER, 0, TIMER_RESOURCE_AMOUNT, amount as c_long as i64);
    }
    s.abc(EE_UNITS_UCP_SET_TIMER, 0, TIMER_ROTTING, time as c_long as i64);
    if s.s(EE_UNITS_UCP_CAPTURE_ACTIVE) == 0 {
        return;
    }
    let tick = s.s(EE_UNITS_UC_TICK_MS) as u32;
    let after = s.b(EE_UNITS_UCP_TIMER, TIMER_ROTTING) as u32;
    let amount_after = s.b(EE_UNITS_UCP_TIMER, TIMER_RESOURCE_AMOUNT) as u32;
    h.note_step(STEP_KIND_RESOURCES_ROTTING, id, &[before as u32, amount_before as u32, tick, after, amount_after]);
}

/// Runs body `what` of `class` on `unit` (`a` an object, `b` a value); the body's answer, or 0.
pub fn run<H: UcHost + UcpRust + ?Sized>(h: &mut H, what: i32, class: u64, unit: u64, a: u64, b: i64) -> i64 {
    match what {
        EE_UNITS_UCP_DO_PLAY_UNIT_EFFECT => play_unit_effect(h, class, unit, b),
        EE_UNITS_UCP_DO_UPDATE_CITIZEN => update_citizen(h, class, unit),
        EE_UNITS_UCP_DO_UPDATE_CONVERSION => update_conversion(h, class, unit),
        EE_UNITS_UCP_DO_CONVERT_TARGET => convert_target(h, class, unit, a),
        EE_UNITS_UCP_DO_CAN_CONVERT => {
            let (result, flag) = can_convert(h, unit, a);
            // the answer: bit 0 the result, bit 1 a written flag, bit 2 its value
            return i64::from(result) | flag.map_or(0, |f| 2 | (i64::from(f) << 2));
        }
        EE_UNITS_UCP_DO_SHOW_CONVERSION => show_conversion(h, class, unit),
        EE_UNITS_UCP_DO_HIDE_CONVERSION => hide_conversion(h, class, unit),
        EE_UNITS_UCP_DO_RELOAD_TIMER => countdown_timer(h, unit, false),
        EE_UNITS_UCP_DO_TARGET_SEARCH_TIMER => countdown_timer(h, unit, true),
        EE_UNITS_UCP_DO_HEAL_SEARCH_TIMER => heal_search_timer(h, unit),
        EE_UNITS_UCP_DO_ROTTING_TIME => return rotting_time(h, unit),
        EE_UNITS_UCP_DO_RESOURCES_ROTTING => resources_rotting(h, unit),
        _ => {}
    }
    0
}

#[cfg(not(test))]
pub(crate) mod game {
    use super::*;
    use std::os::raw::c_void;

    extern "C" {
        fn ee_units_ucp_host(
            op: i32,
            class: *mut c_void,
            unit: *mut c_void,
            a: u64,
            b: i64,
            c: i64,
            f: *mut [f32; 4],
        ) -> i64;
    }

    /// The people's host (`ee_units_ucp_host`; the class host's statements pass through it).
    struct GameP;

    impl UcHost for GameP {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            if let Some(v) = crate::units::tech_cases::ucp(op, unit, a, b) {
                return v;
            }
            if let Some(v) =
                crate::units::uc_view::uc_view(crate::units::uc_view::Kind::People, op, class, unit, a, b, c, f)
            {
                return v;
            }
            // SAFETY: the body runs inside the C++ member that handed over its live class and unit
            unsafe { ee_units_ucp_host(op, class as usize as *mut c_void, unit as usize as *mut c_void, a, b, c, f) }
        }
    }

    impl UcpRust for GameP {
        fn countdown(&mut self, id: u32, object: u64, which: u32, member: i64, tick_ms: i64) -> Option<i64> {
            let mut out = 0i64;
            // SAFETY: the output is live for the call
            let written =
                unsafe { crate::units::step::ee_units_step_countdown(id, object, which, member, tick_ms, &mut out) };
            written.then_some(out)
        }
        fn heal_step(&mut self, id: u32, object: u64, member: f32, tick_time: f32) -> Option<f32> {
            let mut out = 0.0f32;
            // SAFETY: the output is live for the call
            let written = unsafe { crate::units::step::ee_units_step_heal(id, object, member, tick_time, &mut out) };
            written.then_some(out)
        }
        fn rotting(&mut self, id: u32, object: u64, member: i64, tick_ms: i64) -> i64 {
            crate::units::step::ee_units_step_rotting(id, object, member, tick_ms)
        }
        fn resources_rotting(
            &mut self,
            id: u32,
            object: u64,
            time: i64,
            amount: i64,
            tick_ms: i64,
        ) -> (i64, Option<i64>) {
            let (mut t, mut a) = (0i64, 0i64);
            // SAFETY: the outputs are live for the call
            let lost = unsafe {
                crate::units::step::ee_units_step_resources_rotting(id, object, time, amount, tick_ms, &mut t, &mut a)
            };
            (t, lost.then_some(a))
        }
        fn note_step(&mut self, kind: u32, id: u32, words: &[u32]) {
            // SAFETY: the words are live for the call
            unsafe { crate::units::notes::ee_units_note_step(kind, id, words.as_ptr(), words.len() as u32) }
        }
    }

    /// The people's body `what` (`EE_UNITS_UCP_DO_*`) of `class` (or NULL) on `unit`: `a` an object, `b` a value;
    /// the body's answer, or 0.
    ///
    /// # Safety
    /// `class` NULL or a live `EEUnitClass`, `unit` a live `EEComplexUnit`, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ucp_call(
        what: i32,
        class: *mut c_void,
        unit: *mut c_void,
        a: u64,
        b: i64,
    ) -> i64 {
        run(&mut GameP, what, class as usize as u64, unit as usize as u64, a, b)
    }

    /// `EECanConvert(priest, target, flag)`: whether the priest can convert the target now; `flag` written when the
    /// decision writes it.
    ///
    /// # Safety
    /// `priest` and `target` are live `EEComplexUnit`s, `flag` a live bool, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ucp_can_convert(
        priest: *mut c_void,
        target: *mut c_void,
        flag: *mut bool,
    ) -> bool {
        let (result, written) = can_convert(&mut GameP, priest as usize as u64, target as usize as u64);
        if let Some(value) = written {
            // SAFETY: the caller's flag
            unsafe { *flag = value };
        }
        result
    }
}

#[cfg(test)]
#[path = "ucp_people_tests.rs"]
mod tests;
