//! `EEUnitClass`'s per-tick unit bodies over their C++ statements (the units' lane, round 7; were EEUnitClass.cpp):
//! `Animate` (the clock's decisions animate.rs's, as before), `FireOnAttackFrame`, `TurnTurrets`/`TurnTurretsBack`
//! (the turn turret.rs's), `UpdateTrail`, `UpdateRipples`, `CreateRipples`, `HealGarrison`, `Regenerate` (their
//! CBTM/CBHL/CBRG formulas stay C++ statements, in place), `BaseAddToRegion`/`BaseRemoveFromRegion` (the regions
//! ee_combat's Rust, through its FFI functions), `GetClassesInSet`. As uc_create.rs: each C++ statement one
//! `EE_UNITS_UC_*` case of `ee_units_uc_host`, in the original's order.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::animate::{animate as animate_clock, EeUnitsAnimateIn};
use crate::units::turret::{turn_turrets as turn, EeUnitsTurretPart, EeUnitsTurretsIn};
use crate::units::uc_body::*;
use crate::units::uc_create::*;
use crate::units::uc_produce::*;
use ee_base::math::transform::RotationOrder;
use ee_base::math::types::GeTransformation;
use ee_base::math::um::{floor, um_arctan2};
use std::os::raw::c_long;

// ---- the statements (from 420)
/// `unit->GetAnimation()`
pub const EE_UNITS_UC_ANIMATION: i32 = 420;
/// `unit->mIsAnimationPaused`
pub const EE_UNITS_UC_PAUSED: i32 = 421;
/// `unit->GetModel()->mLoops[b]`
pub const EE_UNITS_UC_MODEL_LOOPS: i32 = 422;
/// `unit->GetModel()->mIsSpeedScaled[b]`
pub const EE_UNITS_UC_MODEL_SPEED_SCALED: i32 = 423;
/// `unit->GetModel()->mSharesFrames[b]`
pub const EE_UNITS_UC_MODEL_SHARES_FRAMES: i32 = 424;
/// `unit->mStats->mCanFireOverWalls`
pub const EE_UNITS_UC_CAN_FIRE_OVER_WALLS: i32 = 425;
/// `unit->IsFoundation()`
pub const EE_UNITS_UC_IS_FOUNDATION: i32 = 426;
/// `unit->mAnimationTime`
pub const EE_UNITS_UC_ANIMATION_TIME: i32 = 427;
/// `unit->GetModel()->mHoldTimes[b]`
pub const EE_UNITS_UC_MODEL_HOLD_TIME: i32 = 428;
/// `unit->GetModel()->mStartFrames[b]`
pub const EE_UNITS_UC_MODEL_START_FRAME: i32 = 429;
/// `unit->GetModel()->mLastFrames[b]`
pub const EE_UNITS_UC_MODEL_LAST_FRAME: i32 = 430;
/// `unit->GetModel()->mFrameCounts[b]`
pub const EE_UNITS_UC_MODEL_FRAME_COUNT: i32 = 431;
/// `unit->GetModel()->mFrameTimes[b]`
pub const EE_UNITS_UC_MODEL_FRAME_TIME: i32 = 432;
/// `unit->GetModel()->mSound1Frames[b]`
pub const EE_UNITS_UC_MODEL_SOUND1_FRAME: i32 = 433;
/// `unit->GetModel()->mSound2Frames[b]`
pub const EE_UNITS_UC_MODEL_SOUND2_FRAME: i32 = 434;
/// `f[0] = class->GetMaxVelocity()`
pub const EE_UNITS_UC_MAX_VELOCITY: i32 = 435;
/// `f[0] = unit->mAngularVelocity.GetYaw()`
pub const EE_UNITS_UC_YAW_VELOCITY: i32 = 436;
/// `f[0] = class->mMaxAngularVelocity`
pub const EE_UNITS_UC_MAX_ANGULAR_VELOCITY: i32 = 437;
/// `unit->SetIsAnimationPaused(b != 0)`
pub const EE_UNITS_UC_SET_PAUSED: i32 = 438;
/// `unit->SetAnimationFrame(b)`
pub const EE_UNITS_UC_SET_ANIMATION_FRAME: i32 = 439;
/// `unit->SetAnimationTime(b)`
pub const EE_UNITS_UC_SET_ANIMATION_TIME: i32 = 440;
/// `unit->mSound`
pub const EE_UNITS_UC_SOUND_INSTANCE: i32 = 441;
/// `gSoundManager->Stop(instance a, !gSoundManager->GetIsLooping(instance a))`
pub const EE_UNITS_UC_STOP_SOUND: i32 = 442;
/// `unit->ReleaseSound()`
pub const EE_UNITS_UC_RELEASE_SOUND: i32 = 443;
/// `f[0] = gPhysics->GetRandom()`
pub const EE_UNITS_UC_RANDOM: i32 = 444;
/// `class->mIsHorse`
pub const EE_UNITS_UC_IS_HORSE: i32 = 445;
/// `unit->GetModel()->GetTagPoints(b, c)`
pub const EE_UNITS_UC_MODEL_TAGS: i32 = 446;
/// `f[0] = gWorld.mTimeOfDay`
pub const EE_UNITS_UC_TIME_OF_DAY: i32 = 447;
/// tile `a`'s `mTerrainType`
pub const EE_UNITS_UC_TILE_TERRAIN: i32 = 448;
/// `gWorld.mEnvironmentManager->IsInStorm(U2DPoint<long>(unit->GetTileX(), unit->GetTileY()), false, true)`
pub const EE_UNITS_UC_IN_STORM: i32 = 449;
/// `EEIsSeen(unit)`
pub const EE_UNITS_UC_IS_SEEN: i32 = 450;
/// `gEffectManager->CreateEffectBetweenUnits(unit, unit, EEGraphicEffect::Get(kGraphicEffectHorseBreathUnits), NULL)`
pub const EE_UNITS_UC_HORSE_BREATH: i32 = 451;
/// `unit->GetModel()->mSounds1[b]`
pub const EE_UNITS_UC_MODEL_SOUNDS1_AT: i32 = 452;
/// `unit->GetModel()->mSounds2[b]`
pub const EE_UNITS_UC_MODEL_SOUNDS2_AT: i32 = 453;
/// `f[0] = unit->mAnimationFraction`
pub const EE_UNITS_UC_FRACTION: i32 = 454;
/// `unit->mAnimationFraction = f[0]`
pub const EE_UNITS_UC_SET_FRACTION: i32 = 455;
/// `unit->SetNextFrame(b)`
pub const EE_UNITS_UC_SET_NEXT_FRAME: i32 = 456;
/// `gSoundManager->GetIsLooping(instance a)`
pub const EE_UNITS_UC_IS_LOOPING: i32 = 457;
/// `gSoundManager->PlayAndReturnReferencedNode(sound a->GetSampleID(), f[0], f[1], f[2], sound a->GetLooping(),
/// class->mName)`
pub const EE_UNITS_UC_PLAY_REFERENCED: i32 = 458;
/// `unit->SetSound(instance a)`
pub const EE_UNITS_UC_SET_SOUND: i32 = 459;
/// instance `a`'s `Dereference()`
pub const EE_UNITS_UC_NODE_DEREFERENCE: i32 = 460;
/// `EEGetAttackFrameSet(unit)`
pub const EE_UNITS_UC_ATTACK_FRAME_SET: i32 = 461;
/// `class->FireShot(unit)`
pub const EE_UNITS_UC_FIRE_SHOT: i32 = 462;
/// `f[0] = unit->mStats->mTurretMaxAngularVelocity`
pub const EE_UNITS_UC_TURRET_RATE: i32 = 463;
/// `f[0] = gPhysics->mTickSeconds`
pub const EE_UNITS_UC_TICK_SECONDS: i32 = 464;
/// `unit->GetModel()->mTurretParts.size()`
pub const EE_UNITS_UC_TURRET_PART_COUNT: i32 = 465;
/// `f[0..3] = unit->GetModel()->mTurretParts[b]->mPosition`
pub const EE_UNITS_UC_TURRET_PART_POSITION: i32 = 466;
/// the transformation at `a` = `unit->GetModel()->mTurretParts[b]->mTransformation`
pub const EE_UNITS_UC_TURRET_PART_TRANSFORM: i32 = 467;
/// `unit->mTurretAngles[b] = f[0]`
pub const EE_UNITS_UC_SET_TURRET_ANGLE: i32 = 468;
/// `unit->mTrailEffectTimer`
pub const EE_UNITS_UC_TRAIL_TIMER: i32 = 469;
/// `unit->SetTrailEffectTimer(b)`
pub const EE_UNITS_UC_SET_TRAIL_TIMER: i32 = 470;
/// `gEffectManager->CreateUnitEffect(unit, class->mTrailEffect, NULL, 0, 0, 0)`
pub const EE_UNITS_UC_TRAIL_EFFECT_ON: i32 = 471;
/// `unit->SetTrailEffectTimer(class->mTrailEffect->mTime1)`
pub const EE_UNITS_UC_RESET_TRAIL_TIMER: i32 = 472;
/// `unit->mWakeTimer`
pub const EE_UNITS_UC_WAKE_TIMER: i32 = 473;
/// `unit->mWakeTimer = b`
pub const EE_UNITS_UC_SET_WAKE_TIMER: i32 = 474;
/// `f[0] = unit->GetInverseMaxVelocity()`
pub const EE_UNITS_UC_INVERSE_MAX_VELOCITY: i32 = 475;
/// `class->mRippleEffect->mTime1`
pub const EE_UNITS_UC_RIPPLE_TIME1: i32 = 476;
/// `class->mRippleEffect->mTime2`
pub const EE_UNITS_UC_RIPPLE_TIME2: i32 = 477;
/// `unit->GetModel()->GetTagPoints(unit->GetAnimation(), 0x22)`
pub const EE_UNITS_UC_RIPPLE_TAGS: i32 = 478;
/// tag point list `a`'s `mPoints.size()`
pub const EE_UNITS_UC_TAGS_SIZE: i32 = 479;
/// `EECombatIsSeenBy(unit, gWorld.GetCurrentPlayer(), true)`
pub const EE_UNITS_UC_SEEN_BY_CURRENT: i32 = 480;
/// tag point list `a`'s `Scale(the transformation at b, unit->mPhysicalModel.GetScale())`
pub const EE_UNITS_UC_TAGS_SCALE: i32 = 481;
/// tag point list `a`'s `GetTagPoint(b)->GetPoint(unit->mPhysicalModel.GetFrame(), the transformation at c, f[0..3])`
pub const EE_UNITS_UC_TAG_POINT_AT: i32 = 482;
/// `f[0] = ` tile `a`'s `mHeight`
pub const EE_UNITS_UC_TILE_HEIGHT_OF: i32 = 483;
/// `gEffectManager->CreateTransformationEffect(the transformation at a, class->mRippleEffect, NULL)`
pub const EE_UNITS_UC_RIPPLE_EFFECT_AT: i32 = 484;
/// `unit->mStats->mHealingPoints`
pub const EE_UNITS_UC_HEALING_POINTS: i32 = 485;
/// `unit->GetNumCargoUnits()`
pub const EE_UNITS_UC_CARGO_COUNT: i32 = 486;
/// `gWorld.mInCinematic`
pub const EE_UNITS_UC_WORLD_IN_CINEMATIC: i32 = 487;
/// the timer scratch = `EECombatTimer({unit->mRegenerationTimer, gPhysics->mTickTimeMs, the rules'
/// mRegenerationInterval})` (CBTM); its `due`
pub const EE_UNITS_UC_COMBAT_TIMER: i32 = 488;
/// the timer scratch's `timer`
pub const EE_UNITS_UC_TIMER_VALUE: i32 = 489;
/// `unit->GetHitPoints()`
pub const EE_UNITS_UC_GET_HIT_POINTS: i32 = 490;
/// `unit->GetCargo(b)`
pub const EE_UNITS_UC_CARGO_AT: i32 = 491;
/// the heal scratch = `EECombatHeal({complex unit a->GetHitPoints(), unit->mStats->mHealingPoints, complex unit
/// a->GetStats()->mMaxHitPoints})` (CBHL); its `set`
pub const EE_UNITS_UC_COMBAT_HEAL: i32 = 492;
/// the heal scratch's `hit_points`
pub const EE_UNITS_UC_HEAL_HIT_POINTS: i32 = 493;
/// `unit->GetPlayer() == gWorld.GetCurrentPlayer()`
pub const EE_UNITS_UC_IS_CURRENT_PLAYER: i32 = 494;
/// `unit->GetPlayer()->mActiveGroup->Contains(unit)`
pub const EE_UNITS_UC_GROUP_CONTAINS: i32 = 495;
/// `gUIManager->GetMainGameForm()->mGroups[0]->Reset()`
pub const EE_UNITS_UC_RESET_GROUP_PANEL: i32 = 496;
/// `unit->SetRegenerationTimer(b)`
pub const EE_UNITS_UC_SET_REGENERATION_TIMER: i32 = 497;
/// the regeneration scratch = `EECombatRegenerate({unit->mRegenerationTimer, gPhysics->mTickTimeMs,
/// unit->mHitPoints, unit->mStats->mRegeneration, unit->mStats->mMaxHitPoints, the rules' mRegenerationInterval})`
/// (CBRG); its `set`
pub const EE_UNITS_UC_COMBAT_REGENERATE: i32 = 498;
/// the regeneration scratch's `hit_points`
pub const EE_UNITS_UC_REGENERATE_HIT_POINTS: i32 = 499;
/// the regeneration scratch's `timer`
pub const EE_UNITS_UC_REGENERATE_TIMER: i32 = 500;
/// `unit->SetHitPoints(b)`
pub const EE_UNITS_UC_SET_HIT_POINTS_SELF: i32 = 501;
/// `unit->mPlayer == gWorld.GetCurrentPlayer()`
pub const EE_UNITS_UC_PLAYER_IS_CURRENT: i32 = 502;
/// `unit->mPlayer->mActiveGroup->Contains(unit)`
pub const EE_UNITS_UC_PLAYER_GROUP_CONTAINS: i32 = 503;
/// simple unit `a`'s `mTileNode`
pub const EE_UNITS_UC_TILE_NODE: i32 = 504;
/// simple unit `a`'s `IsPathMarker()`
pub const EE_UNITS_UC_IS_PATH_MARKER: i32 = 505;
/// `gWorld.mMap->mUnitRegions`
pub const EE_UNITS_UC_UNIT_REGIONS: i32 = 506;
/// the classes vector `a`'s `clear()`
pub const EE_UNITS_UC_CLASSES_CLEAR: i32 = 507;
/// `EEUnitClassCount()`
pub const EE_UNITS_UC_CLASS_COUNT: i32 = 508;
/// `gUnitSetManager.Contains(b, c) == true`
pub const EE_UNITS_UC_SET_CONTAINS: i32 = 509;
/// the classes vector `a`'s `push_back(class b)`
pub const EE_UNITS_UC_CLASSES_PUSH: i32 = 510;

// ---- the bodies (`ee_units_uc_call`'s `what`, from 40)
pub const EE_UNITS_UC_DO_ANIMATE: i32 = 40;
pub const EE_UNITS_UC_DO_FIRE_ON_ATTACK_FRAME: i32 = 41;
pub const EE_UNITS_UC_DO_TURN_TURRETS: i32 = 42;
pub const EE_UNITS_UC_DO_TURN_TURRETS_BACK: i32 = 43;
pub const EE_UNITS_UC_DO_UPDATE_TRAIL: i32 = 44;
pub const EE_UNITS_UC_DO_UPDATE_RIPPLES: i32 = 45;
pub const EE_UNITS_UC_DO_CREATE_RIPPLES: i32 = 46;
pub const EE_UNITS_UC_DO_HEAL_GARRISON: i32 = 47;
pub const EE_UNITS_UC_DO_REGENERATE: i32 = 48;
pub const EE_UNITS_UC_DO_ADD_TO_REGION: i32 = 49;
pub const EE_UNITS_UC_DO_REMOVE_FROM_REGION: i32 = 50;
pub const EE_UNITS_UC_DO_CLASSES_IN_SET: i32 = 51;

const GOAL_ATTACK_UNIT: i64 = 2;
const GOAL_ATTACK_GROUND: i64 = 3;
const GOAL_CAST_CALAMITY: i64 = 11;
const GOAL_HEAL_UNIT: i64 = 15;
const TERRAIN_SNOW: i64 = 5;
const TERRAIN_DEEP_WATER: i64 = 1;
const TERRAIN_SHALLOW_WATER: i64 = 10;
/// the horse breath's tag point kind and the ripples'
const TAGS_HORSE_BREATH: i64 = 0x16;

/// The unit-region table's Rust (ee_combat's FFI functions, as the C++ called them).
pub trait UcRegions {
    fn regions_size(&mut self, table: u64) -> i64;
    fn regions_add(&mut self, table: u64, x: u32, y: u32, unit: u64);
    fn regions_remove(&mut self, table: u64, x: u32, y: u32, unit: u64);
}

/// `Animate(unit)`: the unit's animation advanced by this tick (the clock animate.rs's), the idle pick and the
/// horse's breath, the frame sounds; the number of frames stepped.
pub fn animate<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> i64 {
    let mut s = S { h, class, unit };
    let state = s.s(EE_UNITS_UC_ANIMATION);
    let mut i = EeUnitsAnimateIn { paused: s.s(EE_UNITS_UC_PAUSED) != 0, ..Default::default() };
    i.loops = s.b(EE_UNITS_UC_MODEL_LOOPS, state) != 0;
    i.speed_scaled = s.b(EE_UNITS_UC_MODEL_SPEED_SCALED, state) != 0;
    i.shares_frames = s.b(EE_UNITS_UC_MODEL_SHARES_FRAMES, state) != 0;
    i.can_fire_over_walls =
        if i.paused && i.loops && state == 8 { s.s(EE_UNITS_UC_CAN_FIRE_OVER_WALLS) != 0 } else { false };
    i.foundation = if !i.paused && !i.loops { s.s(EE_UNITS_UC_IS_FOUNDATION) != 0 } else { false };
    i.state = state as u32;
    i.tick_ms = s.s(EE_UNITS_UC_TICK_MS) as u32;
    i.time = s.s(EE_UNITS_UC_ANIMATION_TIME) as u32;
    i.frame = s.s(EE_UNITS_UC_FRAME) as u32;
    i.hold_time = s.b(EE_UNITS_UC_MODEL_HOLD_TIME, state) as u32;
    i.start_frame = s.b(EE_UNITS_UC_MODEL_START_FRAME, state) as u32;
    i.last_frame = s.b(EE_UNITS_UC_MODEL_LAST_FRAME, state) as u32;
    i.frame_count = s.b(EE_UNITS_UC_MODEL_FRAME_COUNT, state) as u32;
    i.frame_time = s.b(EE_UNITS_UC_MODEL_FRAME_TIME, state) as u32;
    i.sound1_frame = s.b(EE_UNITS_UC_MODEL_SOUND1_FRAME, state) as u32;
    i.sound2_frame = s.b(EE_UNITS_UC_MODEL_SOUND2_FRAME, state) as u32;
    if !i.paused && i.speed_scaled {
        i.velocity = s.float(EE_UNITS_UC_GET_VELOCITY, 0, 0);
        i.max_velocity = s.float(EE_UNITS_UC_MAX_VELOCITY, 0, 0);
        i.yaw_velocity = s.float(EE_UNITS_UC_YAW_VELOCITY, 0, 0);
        i.max_angular_velocity = s.float(EE_UNITS_UC_MAX_ANGULAR_VELOCITY, 0, 0);
    }
    let o = animate_clock(&i);
    if o.set_paused {
        s.b(EE_UNITS_UC_SET_PAUSED, i64::from(o.paused));
    }
    if o.set_frame {
        s.b(EE_UNITS_UC_SET_ANIMATION_FRAME, i64::from(o.frame));
    }
    if o.set_time {
        s.b(EE_UNITS_UC_SET_ANIMATION_TIME, i64::from(o.time));
    }
    if o.stop_sound {
        let instance = s.s(EE_UNITS_UC_SOUND_INSTANCE) as u64;
        if instance != 0 {
            s.a(EE_UNITS_UC_STOP_SOUND, instance);
            s.s(EE_UNITS_UC_RELEASE_SOUND);
        }
    }
    if o.idle_pick {
        let random = s.float(EE_UNITS_UC_RANDOM, 0, 0);
        if random < 0.5 || random.is_nan() || s.s(EE_UNITS_UC_CAN_FIRE_OVER_WALLS) != 0 {
            s.b(EE_UNITS_UC_SET_ANIMATION, 7);
        } else {
            s.b(EE_UNITS_UC_SET_ANIMATION, 8);
        }
        if s.s(EE_UNITS_UC_IS_HORSE) != 0 && s.abc(EE_UNITS_UC_MODEL_TAGS, 0, state, TAGS_HORSE_BREATH) != 0 {
            let mut allowed = true;
            let tile = s.s(EE_UNITS_UC_UNIT_TILE) as u64;
            let time_of_day = s.float(EE_UNITS_UC_TIME_OF_DAY, 0, 0);
            if time_of_day < 0.45
                && time_of_day > 0.075
                && (tile == 0 || s.a(EE_UNITS_UC_TILE_TERRAIN, tile) != TERRAIN_SNOW)
            {
                allowed = s.s(EE_UNITS_UC_IN_STORM) != 0;
            }
            if allowed && s.s(EE_UNITS_UC_IS_SEEN) != 0 {
                s.s(EE_UNITS_UC_HORSE_BREATH);
            }
        }
    }
    let sound = match o.sound {
        1 => s.b(EE_UNITS_UC_MODEL_SOUNDS1_AT, state) as u64,
        2 => s.b(EE_UNITS_UC_MODEL_SOUNDS2_AT, state) as u64,
        _ => 0,
    };
    if !i.paused {
        if s.float(EE_UNITS_UC_FRACTION, 0, 0) != o.fraction {
            s.with_float(EE_UNITS_UC_SET_FRACTION, 0, o.fraction);
        }
        s.b(EE_UNITS_UC_SET_NEXT_FRAME, i64::from(o.next_frame));
    }
    let steps = i64::from(o.steps);
    if sound == 0 {
        return steps;
    }
    let instance = s.s(EE_UNITS_UC_SOUND_INSTANCE) as u64;
    let sample = s.a(EE_UNITS_UC_SAMPLE_ID, sound);
    let mut f = [0.0f32; 4];
    if sample == s.s(EE_UNITS_UC_SOUND_SAMPLE) {
        s.f(EE_UNITS_UC_SOUND_PARAMETERS, sound, 0, 0, &mut f);
        if s.a(EE_UNITS_UC_IS_LOOPING, instance) != 0 {
            s.f(EE_UNITS_UC_SET_SOUND_PARAMETERS, 0, 0, 0, &mut f);
            return steps;
        }
    } else {
        if instance != 0 {
            s.a(EE_UNITS_UC_STOP_SOUND, instance);
            s.s(EE_UNITS_UC_RELEASE_SOUND);
        }
        s.f(EE_UNITS_UC_SOUND_PARAMETERS, sound, 0, 0, &mut f);
    }
    let played = s.f(EE_UNITS_UC_PLAY_REFERENCED, sound, 0, 0, &mut f) as u64;
    s.a(EE_UNITS_UC_SET_SOUND, played);
    if played != 0 {
        s.a(EE_UNITS_UC_NODE_DEREFERENCE, played);
    }
    steps
}

/// `FireOnAttackFrame(unit, arg, frame)`: a shot fired when an attacking unit's animation steps across one of its
/// attack set's sound frames.
pub fn fire_on_attack_frame<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, arg: i64, frame: i64) {
    let mut s = S { h, class, unit };
    let index = s.s(EE_UNITS_UC_ATTACK_FRAME_SET);
    let first = s.b(EE_UNITS_UC_MODEL_SOUND1_FRAME, index) as u32;
    let last = s.b(EE_UNITS_UC_MODEL_SOUND2_FRAME, index) as u32;
    let old = s.s(EE_UNITS_UC_FRAME) as u32;
    let frame = frame as u32;
    if s.s(EE_UNITS_UC_ANIMATION) as u32 == index as u32
        && arg != 0
        && s.s(EE_UNITS_UC_PAUSED) == 0
        && ((first > frame && (old >= first || old <= frame))
            || (frame > first && (old <= frame && old >= first))
            || (last > frame && (old >= last || old <= frame))
            || (frame > last && (old <= frame && old >= last)))
    {
        s.s(EE_UNITS_UC_FIRE_SHOT);
    }
}

/// The turrets' turn for one tick (turret.rs): aim at (`x`, `y`) or turn back; each angle stored back where it
/// differs.
fn turn_turrets_toward<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, aim: bool, x: f32, y: f32) {
    let mut s = S { h, class, unit };
    let mut i = EeUnitsTurretsIn {
        aim,
        alive: s.s(EE_UNITS_UC_HIT_POINTS) > 0,
        turn_rate: s.float(EE_UNITS_UC_TURRET_RATE, 0, 0),
        tick_seconds: s.float(EE_UNITS_UC_TICK_SECONDS, 0, 0),
        scale: s.float(EE_UNITS_UC_STATS_SCALE, 0, 0),
        ..Default::default()
    };
    s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut i.location) as i64);
    i.target_x = x;
    i.target_y = y;
    let count = s.s(EE_UNITS_UC_TURRET_PART_COUNT) as u32 as usize;
    if count == 0 || (!aim && !i.alive) {
        return;
    }
    let mut parts = vec![EeUnitsTurretPart::default(); count];
    for (k, part) in parts.iter_mut().enumerate() {
        let k = k as i64;
        s.point(EE_UNITS_UC_TURRET_PART_POSITION, 0, k, 0, &mut part.position);
        s.ab(EE_UNITS_UC_TURRET_PART_TRANSFORM, addr(&mut part.transform), k);
        part.angle = s.float(EE_UNITS_UC_TURRET_ANGLE, 0, k);
    }
    turn(&i, &mut parts);
    for (k, part) in parts.iter().enumerate() {
        if s.float(EE_UNITS_UC_TURRET_ANGLE, 0, k as i64) != part.angle {
            let mut f = [part.angle, 0.0, 0.0, 0.0];
            s.f(EE_UNITS_UC_SET_TURRET_ANGLE, 0, k as i64, 0, &mut f);
        }
    }
}

/// `TurnTurretsBack(unit)`: each turret of a living unit turned back toward straight ahead.
pub fn turn_turrets_back<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    turn_turrets_toward(h, class, unit, false, 0.0, 0.0);
}

/// `TurnTurrets(unit)`: each turret turned toward the unit's attack, heal or calamity target; else back.
pub fn turn_turrets<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h: &mut *h, class, unit };
    let goal = s.s(EE_UNITS_UC_GOAL_STATE_SELF) as u64;
    let kind = if goal != 0 { s.a(EE_UNITS_UC_GOAL_KIND, goal) } else { -1 };
    if goal == 0
        || (kind != GOAL_ATTACK_UNIT
            && kind != GOAL_ATTACK_GROUND
            && kind != GOAL_CAST_CALAMITY
            && kind != GOAL_HEAL_UNIT)
    {
        turn_turrets_back(h, class, unit);
        return;
    }
    let mut target = [0.0f32; 3];
    if kind == GOAL_ATTACK_UNIT {
        let attacked = s.a(EE_UNITS_UC_ATTACK_TARGET, goal) as u64;
        let complex = s.a(EE_UNITS_UC_COMPLEX_OF, attacked) as u64;
        if complex != 0 {
            s.point(EE_UNITS_UC_LOCATION, complex, 0, 0, &mut target);
        } else {
            target[0] = s.a(EE_UNITS_UC_UNIT_TILE_X, attacked) as c_long as f32;
            target[1] = s.a(EE_UNITS_UC_UNIT_TILE_Y, attacked) as c_long as f32;
        }
    } else {
        s.point(EE_UNITS_UC_GOAL_LOCATION, goal, 0, 0, &mut target);
    }
    turn_turrets_toward(h, class, unit, true, target[0], target[1]);
}

/// `UpdateTrail(unit)`: the trail timer counted down; the class's trail effect each time it runs out.
pub fn update_trail<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    let time = s.s(EE_UNITS_UC_TRAIL_TIMER) as c_long;
    if time > 0 {
        let mut left = time.wrapping_sub(s.s(EE_UNITS_UC_TICK_MS) as c_long);
        if left < 0 {
            left = 0;
        }
        s.b(EE_UNITS_UC_SET_TRAIL_TIMER, left as i64);
    } else {
        s.s(EE_UNITS_UC_TRAIL_EFFECT_ON);
        s.s(EE_UNITS_UC_RESET_TRAIL_TIMER);
    }
}

/// `UpdateRipples(unit)`: ripples behind a unit on water at a rate that grows with its speed.
pub fn update_ripples<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h: &mut *h, class, unit };
    let tile = s.s(EE_UNITS_UC_UNIT_TILE) as u64;
    if tile == 0 {
        return;
    }
    let terrain = s.a(EE_UNITS_UC_TILE_TERRAIN, tile);
    if terrain != TERRAIN_DEEP_WATER && terrain != TERRAIN_SHALLOW_WATER {
        return;
    }
    let time = s.s(EE_UNITS_UC_WAKE_TIMER) as c_long;
    let progress = s.float(EE_UNITS_UC_GET_VELOCITY, 0, 0);
    let rate = s.float(EE_UNITS_UC_INVERSE_MAX_VELOCITY, 0, 0) * progress;
    let maximum = s.s(EE_UNITS_UC_RIPPLE_TIME2) as u32;
    let time1 = s.s(EE_UNITS_UC_RIPPLE_TIME1) as u32;
    if (time as f32) < maximum as f32 - maximum.wrapping_sub(time1) as f32 * rate {
        let new_time = time.wrapping_add(s.s(EE_UNITS_UC_TICK_MS) as c_long);
        if s.s(EE_UNITS_UC_WAKE_TIMER) as c_long != new_time {
            s.b(EE_UNITS_UC_SET_WAKE_TIMER, new_time as i64);
        }
    } else {
        create_ripples(h, class, unit);
        let mut s = S { h, class, unit };
        if s.s(EE_UNITS_UC_WAKE_TIMER) != 0 {
            s.b(EE_UNITS_UC_SET_WAKE_TIMER, 0);
        }
    }
}

/// `CreateRipples(unit)`: the class's ripple effect at the model's ripple tag points under water, when the player
/// sees the unit.
// the original's `!(a < 0.0f)` tests (a NaN height passes them)
#[allow(clippy::neg_cmp_op_on_partial_ord)]
pub fn create_ripples<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    let mut effect = GeTransformation::IDENTITY;
    let mut model = GeTransformation::IDENTITY;
    let tags = s.s(EE_UNITS_UC_RIPPLE_TAGS) as u64;
    if tags == 0 || s.a(EE_UNITS_UC_TAGS_SIZE, tags) == 0 || s.s(EE_UNITS_UC_SEEN_BY_CURRENT) == 0 {
        return;
    }
    s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut model.m) as i64);
    s.ab(EE_UNITS_UC_TAGS_SCALE, tags, addr(&mut model.m) as i64);
    let mut i = 0i64;
    while (i as u64) < s.a(EE_UNITS_UC_TAGS_SIZE, tags) as u64 {
        let mut location = [0.0f32; 3];
        s.point(EE_UNITS_UC_TAG_POINT_AT, tags, i, addr(&mut model.m) as i64, &mut location);
        let y = floor(location[1]);
        let x = floor(location[0]);
        let tile = s.abc(EE_UNITS_UC_GET_TILE, 0, i64::from(x), i64::from(y)) as u64;
        i += 1;
        if tile == 0 || !(s.float(EE_UNITS_UC_TILE_HEIGHT_OF, tile, 0) < 0.0) || !(location[2] < 0.0) {
            continue;
        }
        effect.set_orientation(RotationOrder::Ypr, um_arctan2(model.m[1][0], model.m[0][0]), 0.0, 0.0);
        effect.m[0][3] = location[0];
        effect.m[1][3] = location[1];
        effect.m[2][3] = location[2];
        s.a(EE_UNITS_UC_RIPPLE_EFFECT_AT, addr(&mut effect.m));
    }
}

/// `HealGarrison(unit)`: the living units garrisoned in a living unit healed by its healing points each second.
pub fn heal_garrison<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    if s.s(EE_UNITS_UC_HEALING_POINTS) == 0
        || s.s(EE_UNITS_UC_CARGO_COUNT) == 0
        || s.s(EE_UNITS_UC_WORLD_IN_CINEMATIC) != 0
    {
        return;
    }
    let due = s.s(EE_UNITS_UC_COMBAT_TIMER) != 0;
    let timer = s.s(EE_UNITS_UC_TIMER_VALUE);
    let mut healed = false;
    if due {
        if s.s(EE_UNITS_UC_GET_HIT_POINTS) > 0 {
            let count = s.s(EE_UNITS_UC_CARGO_COUNT) as u32;
            for i in 0..count {
                let cargo = s.b(EE_UNITS_UC_CARGO_AT, i64::from(i)) as u64;
                if s.a(EE_UNITS_UC_COMBAT_HEAL, cargo) != 0 {
                    let hit_points = s.s(EE_UNITS_UC_HEAL_HIT_POINTS);
                    s.ab(EE_UNITS_UC_SET_HIT_POINTS_OF, cargo, hit_points);
                    healed = true;
                }
            }
        }
        if healed && s.s(EE_UNITS_UC_IS_CURRENT_PLAYER) != 0 && s.s(EE_UNITS_UC_GROUP_CONTAINS) != 0 {
            s.s(EE_UNITS_UC_RESET_GROUP_PANEL);
        }
    }
    s.b(EE_UNITS_UC_SET_REGENERATION_TIMER, timer);
}

/// `Regenerate(unit)`: a living unit's hit points regenerated each second up to its maximum (not in cinematics).
pub fn regenerate<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    if s.s(EE_UNITS_UC_WORLD_IN_CINEMATIC) != 0 {
        return;
    }
    let set = s.s(EE_UNITS_UC_COMBAT_REGENERATE) != 0;
    let hit_points = s.s(EE_UNITS_UC_REGENERATE_HIT_POINTS);
    let timer = s.s(EE_UNITS_UC_REGENERATE_TIMER);
    if set {
        s.b(EE_UNITS_UC_SET_HIT_POINTS_SELF, hit_points);
        if s.s(EE_UNITS_UC_PLAYER_IS_CURRENT) != 0 && s.s(EE_UNITS_UC_PLAYER_GROUP_CONTAINS) != 0 {
            s.s(EE_UNITS_UC_RESET_GROUP_PANEL);
        }
    }
    s.b(EE_UNITS_UC_SET_REGENERATION_TIMER, timer);
}

/// `BaseAddToRegion(unit)` (`add`) / `BaseRemoveFromRegion(unit)`: a placed unit (not a path marker) added to or
/// removed from the map's unit region of its tile.
pub fn region<H: UcHost + UcRegions + ?Sized>(h: &mut H, class: u64, simple: u64, add: bool) {
    let table;
    let tile;
    let complex;
    {
        let mut s = S { h: &mut *h, class, unit: 0 };
        if s.a(EE_UNITS_UC_TILE_NODE, simple) == 0 || s.a(EE_UNITS_UC_IS_PATH_MARKER, simple) != 0 {
            return;
        }
        table = s.s(EE_UNITS_UC_UNIT_REGIONS) as u64;
        tile = (s.a(EE_UNITS_UC_UNIT_TILE_X, simple) as c_long, s.a(EE_UNITS_UC_UNIT_TILE_Y, simple) as c_long);
        complex = s.a(EE_UNITS_UC_COMPLEX_OF, simple) as u64;
    }
    let size = h.regions_size(table) as c_long;
    let (x, y) = (tile.0.wrapping_div(size) as u32, tile.1.wrapping_div(size) as u32);
    if add {
        h.regions_add(table, x, y, complex);
    } else {
        h.regions_remove(table, x, y, complex);
    }
}

/// `GetClassesInSet(set, classes)`: every unit class whose unit type is in the set, into `classes` (cleared).
pub fn classes_in_set<H: UcHost + ?Sized>(h: &mut H, set: i64, classes: u64) {
    let mut s = S { h, class: 0, unit: 0 };
    s.a(EE_UNITS_UC_CLASSES_CLEAR, classes);
    let mut i = 0i64;
    while i < s.s(EE_UNITS_UC_CLASS_COUNT) {
        let class = s.b(EE_UNITS_UC_CLASS_AT, i) as u64;
        if class != 0 {
            let unit_type = s.a(EE_UNITS_UC_CLASS_UNIT_TYPE, class);
            if s.abc(EE_UNITS_UC_SET_CONTAINS, 0, unit_type, set) != 0 {
                s.ab(EE_UNITS_UC_CLASSES_PUSH, classes, class as i64);
            }
        }
        i += 1;
    }
}

/// Runs body `what` (`a`, `b`, `c` its arguments); `Animate`'s steps, else 0.
#[allow(clippy::too_many_arguments)]
pub fn run<H: UcHost + UcRegions + ?Sized>(
    h: &mut H,
    what: i32,
    class: u64,
    unit: u64,
    a: u64,
    b: i64,
    c: i64,
    _f: [f32; 4],
) -> Option<i64> {
    Some(match what {
        EE_UNITS_UC_DO_ANIMATE => animate(h, class, unit),
        EE_UNITS_UC_DO_FIRE_ON_ATTACK_FRAME => {
            fire_on_attack_frame(h, class, unit, b, c);
            0
        }
        EE_UNITS_UC_DO_TURN_TURRETS => {
            turn_turrets(h, class, unit);
            0
        }
        EE_UNITS_UC_DO_TURN_TURRETS_BACK => {
            turn_turrets_back(h, class, unit);
            0
        }
        EE_UNITS_UC_DO_UPDATE_TRAIL => {
            update_trail(h, class, unit);
            0
        }
        EE_UNITS_UC_DO_UPDATE_RIPPLES => {
            update_ripples(h, class, unit);
            0
        }
        EE_UNITS_UC_DO_CREATE_RIPPLES => {
            create_ripples(h, class, unit);
            0
        }
        EE_UNITS_UC_DO_HEAL_GARRISON => {
            heal_garrison(h, class, unit);
            0
        }
        EE_UNITS_UC_DO_REGENERATE => {
            regenerate(h, class, unit);
            0
        }
        EE_UNITS_UC_DO_ADD_TO_REGION => {
            region(h, class, a, true);
            0
        }
        EE_UNITS_UC_DO_REMOVE_FROM_REGION => {
            region(h, class, a, false);
            0
        }
        EE_UNITS_UC_DO_CLASSES_IN_SET => {
            classes_in_set(h, b, a);
            0
        }
        _ => return None,
    })
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::combat::regions::{
        ee_combat_regions_add, ee_combat_regions_remove, ee_combat_regions_size, EeCombatUnitRegions,
    };
    use crate::units::uc_body::game::Game;

    fn table_of(table: u64) -> *mut EeCombatUnitRegions {
        table as usize as *mut EeCombatUnitRegions
    }

    impl UcRegions for Game {
        fn regions_size(&mut self, table: u64) -> i64 {
            // SAFETY: the map's live region table
            i64::from(unsafe { ee_combat_regions_size(table_of(table)) })
        }
        fn regions_add(&mut self, table: u64, x: u32, y: u32, unit: u64) {
            // SAFETY: as regions_size
            unsafe { ee_combat_regions_add(table_of(table), x, y, unit) }
        }
        fn regions_remove(&mut self, table: u64, x: u32, y: u32, unit: u64) {
            // SAFETY: as regions_size
            unsafe { ee_combat_regions_remove(table_of(table), x, y, unit) }
        }
    }
}

#[cfg(test)]
#[path = "uc_anim_tests.rs"]
mod tests;
