//! A trigger effect's run (`EESMEffect::Execute` and its helpers): which units, players, areas and objects an effect
//! acts on, its tests and loops, in the original's order, over the world's calls by operation code
//! ([`Host::world`], `EE_SCENARIO_WORLD_*`: each one statement or one statement sequence of the original, answered by
//! the C++ host in `Scenario Manager/EESMEffect.cpp`). The units are passed by id (the object lists hold ids), the
//! players by index; the C++ keeps the objects a run builds up (the AI's task and strategy, an active group, a goal,
//! the calamity point, the placement's location) in the run's context.
//!
//! The AI's effects are [`crate::effect_ai`], the two task effects [`crate::task`].

#![allow(clippy::unnecessary_cast, clippy::collapsible_match)]

use std::os::raw::{c_long, c_ulong};

use crate::aivar::{ai_variable, EeScenarioAiWrite};
use crate::class_attribute::modify_unit_attribute;
use crate::condition::{choose_player_recorded, shuffle};
use crate::fields::EeScenarioEffectFields;
use crate::host::*;
use crate::placement::{
    EE_SCENARIO_READ_MAP_TILE_EXISTS, EE_SCENARIO_READ_OBJECT_UNIT_AT, EE_SCENARIO_READ_OBJECT_UNIT_COUNT,
    EE_SCENARIO_TILES_AREA,
};
use crate::player::EE_SCENARIO_WRITE_SET_ATTRIBUTE;

// ---- the world's reads ----
/// A unit's value (`a` the unit id, `b` an `EE_SCENARIO_UV_*`); the unit must exist except for `EXISTS`.
pub const EE_SCENARIO_WORLD_UNIT: u32 = 0;
/// `gWorld.mCurrentPlayer`.
pub const EE_SCENARIO_WORLD_CURRENT_PLAYER: u32 = 1;
/// `gInCinematic` (the free camera's cinematic flag).
pub const EE_SCENARIO_WORLD_IN_CINEMATIC: u32 = 2;
/// `GetPlayer(a)->mObserverMode`.
pub const EE_SCENARIO_WORLD_PLAYER_OBSERVER: u32 = 3;
/// `GetPlayer(a)->IsHuman()`.
pub const EE_SCENARIO_WORLD_PLAYER_IS_HUMAN: u32 = 4;
/// `dynamic_cast<EEComputerPlayer*>(GetPlayer(a)) != NULL`.
pub const EE_SCENARIO_WORLD_PLAYER_IS_COMPUTER: u32 = 5;
/// `GetArea(a)`: -1 for none, else bit 0 `IsUnset()`, bit 1 `IsOffMap()`.
pub const EE_SCENARIO_WORLD_AREA_STATE: u32 = 6;
/// `GetArea(a)->GetCenterTile()`, packed (x low, y high).
pub const EE_SCENARIO_WORLD_AREA_CENTER: u32 = 7;
/// `GetScenarioObject(a) != NULL ? Evaluate(false) : -1`.
pub const EE_SCENARIO_WORLD_OBJECT_EVALUATE: u32 = 8;
/// `!mIsShowingDialog && GetNumMessages() == 0` (the manager's).
pub const EE_SCENARIO_WORLD_DIALOG_IDLE: u32 = 9;
/// Unit class `a`'s tests for the favourite units (`EE_SCENARIO_CLASS_*` bits; 0 for no class).
pub const EE_SCENARIO_WORLD_CLASS: u32 = 10;
/// `GetScenarioObject(a)->PickUnitType()`.
pub const EE_SCENARIO_WORLD_OBJECT_PICK: u32 = 11;
/// The map tile (a, b): -1 for none, else its height's bits.
pub const EE_SCENARIO_WORLD_TILE_HEIGHT: u32 = 12;

// ---- the object effects ----
/// `CreateUnits()` (create.rs over its own host).
pub const EE_SCENARIO_WORLD_CREATE_UNITS: u32 = 20;
/// Unit a: `Kill(b, false, false)`, then off the map `GetPlayer()->KillUnit(unit, false)`.
pub const EE_SCENARIO_WORLD_KILL: u32 = 21;
/// Unit a: `ClearGoals(b)`.
pub const EE_SCENARIO_WORLD_CLEAR_GOALS: u32 = 22;
/// Unit a: `ClearGoalQueue(b)`.
pub const EE_SCENARIO_WORLD_CLEAR_QUEUE: u32 = 23;
/// Unit a: `StartAction(b)`, then its class's `ActionChanged(unit)`.
pub const EE_SCENARIO_WORLD_ACTION: u32 = 24;
/// Unit a: `SetIsDeletable(b)`.
pub const EE_SCENARIO_WORLD_SET_DELETABLE: u32 = 25;
/// Unit a: `SetDisabled(b, 0)`.
pub const EE_SCENARIO_WORLD_SET_DISABLED: u32 = 26;
/// Unit a: `SetIsCommandable(b)`.
pub const EE_SCENARIO_WORLD_SET_COMMANDABLE: u32 = 27;
/// `EEConvertUnit(unit a, b, 0)`.
pub const EE_SCENARIO_WORLD_CONVERT: u32 = 28;
/// Unit a: whether its yaw is at least a degree from `b` (an `f32`'s bits).
pub const EE_SCENARIO_WORLD_FACE_TEST: u32 = 29;
/// Unit a: turned to yaw `b` at once (transformation, ground).
pub const EE_SCENARIO_WORLD_FACE_SET: u32 = 30;
/// Unit a: a turn goal to yaw `b`.
pub const EE_SCENARIO_WORLD_FACE_GOAL: u32 = 31;
/// Unit a's stats take the effect's new name.
pub const EE_SCENARIO_WORLD_RENAME_UNIT: u32 = 32;
/// Player a's upgrade of class b's unit type takes the effect's new name.
pub const EE_SCENARIO_WORLD_RENAME_CLASS: u32 = 33;
/// The main game form's group panel refreshed.
pub const EE_SCENARIO_WORLD_REFRESH_PANEL: u32 = 34;
/// Unit a: explores (a new active group).
pub const EE_SCENARIO_WORLD_STANCE_EXPLORE: u32 = 35;
/// Unit a (and a gate's second and fourth parts): locked (b 1) or unlocked.
pub const EE_SCENARIO_WORLD_LOCK: u32 = 36;
/// Unit a: patrols its nearest town.
pub const EE_SCENARIO_WORLD_STANCE_TOWN_PATROL: u32 = 37;
/// Unit a: `mBehavior = EEUnitBehavior::sTable[b]`.
pub const EE_SCENARIO_WORLD_BEHAVIOR: u32 = 38;
/// `EEChangeUnit(c, d, unit a (none for -1), e, 0, false, f)`: c the attribute, d the percent flag, e the amount,
/// f the mode.
pub const EE_SCENARIO_WORLD_CHANGE_UNIT: u32 = 39;

// ---- the player effects ----
/// Player a: `SetGameResult(b ? 1 : 2)`, `RemovePlayerFromGame(b, a)`.
pub const EE_SCENARIO_WORLD_PLAYER_RESULT: u32 = 42;
/// Player a's `mComputerPlayer->SetActive(b)`.
pub const EE_SCENARIO_WORLD_COMPUTER_SET_ACTIVE: u32 = 43;
/// `SetDiplomaticStance(a, b, c)`.
pub const EE_SCENARIO_WORLD_SET_STANCE: u32 = 44;
/// Player a: `NoticeTargetPlayer(GetPlayer(b), false)`.
pub const EE_SCENARIO_WORLD_NOTICE_TARGET: u32 = 45;
/// `PostFadeIn(3000)` (a 1) or `PostFadeOut(3000)`.
pub const EE_SCENARIO_WORLD_FADE: u32 = 46;
/// Player a: the effect's technology added (made available; past the epochs researched, with its media).
pub const EE_SCENARIO_WORLD_TECH_ADD: u32 = 47;
/// Player a: the effect's technology made obsolete (its unit class's queued production replaced).
pub const EE_SCENARIO_WORLD_TECH_REMOVE: u32 = 48;
/// `AddV134Effect(*this)` (a pending effect).
pub const EE_SCENARIO_WORLD_ADD_PENDING: u32 = 49;
/// Player a: `mSeesAll = b`, the mini map redrawn for the current player.
pub const EE_SCENARIO_WORLD_SEES_ALL: u32 = 50;
/// Player a: b civilization points, the builder button blinking, the trumpets.
pub const EE_SCENARIO_WORLD_CIV_POINTS: u32 = 51;
/// Unit a's tile, or its container's: packed, -1 when the tile is NULL.
pub const EE_SCENARIO_WORLD_FLARE_UNIT_TILE: u32 = 52;
/// Player a: a mini map flare at tile (b, c).
pub const EE_SCENARIO_WORLD_FLARE: u32 = 53;
/// Trigger a: b 0 `mConditionStartTime`, 1 `mIsOn`, 2 `mHasFired` = c.
pub const EE_SCENARIO_WORLD_TRIGGER_SET: u32 = 54;

// ---- the media effects ----
/// The effect's sound file played once.
pub const EE_SCENARIO_WORLD_PLAY_SOUND: u32 = 55;
/// The media text a (hints, instructions, history) cleared (b 1) or extended by the description; trumpets, blinking.
pub const EE_SCENARIO_WORLD_CHANGE_TEXT: u32 = 56;
/// The effect's message with its tags expanded, kept for the dialogue or chat.
pub const EE_SCENARIO_WORLD_EXPAND_MESSAGE: u32 = 57;
/// The current player's dialogue message (a the cinematic flag) queued.
pub const EE_SCENARIO_WORLD_DIALOGUE: u32 = 58;
/// `DisplayMessage` of the run's empty media text (another player's dialogue).
pub const EE_SCENARIO_WORLD_DISPLAY_EMPTY: u32 = 59;
/// The chat log's entry for the expanded message over the effect's duration.
pub const EE_SCENARIO_WORLD_CHAT_LOG: u32 = 60;
/// A message box with the expanded message.
pub const EE_SCENARIO_WORLD_CHAT_BOX: u32 = 61;

// ---- the game effects ----
/// `SetTimeOfDay(a's f32)`.
pub const EE_SCENARIO_WORLD_SET_TIME_OF_DAY: u32 = 62;
/// `gPhysics->mTimeOfDayRuns = a`.
pub const EE_SCENARIO_WORLD_TIME_OF_DAY_RUNS: u32 = 63;
/// Calamity class `a` kept for the run: whether there is one.
pub const EE_SCENARIO_WORLD_CAL_CLASS: u32 = 64;
/// `GetCalamityType(a, type)`: the type, -1 when it answers false.
pub const EE_SCENARIO_WORLD_CAL_TYPE: u32 = 65;
/// The kept class's `CanCastAt(NULL, the run's point, a, b, true, unit c or NULL for -1) == 0`.
pub const EE_SCENARIO_WORLD_CAL_CAN_CAST: u32 = 66;
/// `CreateGlobalCalamity(a, 0, 0)`.
pub const EE_SCENARIO_WORLD_CAL_GLOBAL: u32 = 67;
/// `CreateLocationCalamity(a, -1, b, c, 0)`.
pub const EE_SCENARIO_WORLD_CAL_LOCATION: u32 = 68;
/// `CreateUnitCalamity(a, -1, b, 0)`.
pub const EE_SCENARIO_WORLD_CAL_UNIT: u32 = 69;
/// The units on map tile (b, c), each once a run, that the kept class can cast at: `CreateUnitCalamity(a, ...)`.
pub const EE_SCENARIO_WORLD_CAL_TILE_UNITS: u32 = 70;
/// `EEGraphicEffect::Get(a)` kept and loaded: -1 for none, else `EE_SCENARIO_GFX_*` bits of its kind.
pub const EE_SCENARIO_WORLD_GFX_BEGIN: u32 = 71;
/// The kept effect as weather over tiles (a: a pointer to x, y `i32` pairs, b their count).
pub const EE_SCENARIO_WORLD_GFX_WEATHER: u32 = 72;
/// The kept effect placed at tile (a, b) (its height; 0 on water).
pub const EE_SCENARIO_WORLD_GFX_PLACED: u32 = 73;
/// The kept effect between units a and b.
pub const EE_SCENARIO_WORLD_GFX_BETWEEN: u32 = 74;
/// The kept effect on unit a.
pub const EE_SCENARIO_WORLD_GFX_UNIT: u32 = 75;
/// The kept effect unloaded.
pub const EE_SCENARIO_WORLD_GFX_END: u32 = 76;
/// `StartCinematic()`.
pub const EE_SCENARIO_WORLD_START_CINEMATIC: u32 = 77;
/// `SetGameSpeed(a's f32)`.
pub const EE_SCENARIO_WORLD_SET_GAME_SPEED: u32 = 78;
/// `EndCinematic()`.
pub const EE_SCENARIO_WORLD_END_CINEMATIC: u32 = 79;
/// `SetTimedEffectHasRun(this, false)`.
pub const EE_SCENARIO_WORLD_TIMED_NOT_RUN: u32 = 80;
/// The environment manager's `SetEnabled(a)` (when there is one).
pub const EE_SCENARIO_WORLD_WEATHER: u32 = 81;

// ---- the AI effects ----
/// Computer player a's toggle b (`kSMAIToggle*`) set to c.
pub const EE_SCENARIO_WORLD_AI_TOGGLE: u32 = 82;
/// Computer player a's favourite air (b 0), land (1) or sea (2) unit type c.
pub const EE_SCENARIO_WORLD_AI_FAVORITE: u32 = 83;
/// Computer player a: one Set AI Variable store (b: a pointer to an `EeScenarioAiWrite`).
pub const EE_SCENARIO_WORLD_AI_STORE: u32 = 84;
/// A copy of the effect for player value a as a pending effect.
pub const EE_SCENARIO_WORLD_ADD_PENDING_PLAYER: u32 = 85;

// ---- the camera ----
/// `ResetPath()`.
pub const EE_SCENARIO_WORLD_CAMERA_RESET: u32 = 86;
/// `mTurnYaw = 0`.
pub const EE_SCENARIO_WORLD_CAMERA_TURN_RESET: u32 = 87;
/// `SetMode(3)`, `SetZoom(a's f32)`.
pub const EE_SCENARIO_WORLD_CAMERA_ZOOM: u32 = 88;
/// A path key at unit a over b ms.
pub const EE_SCENARIO_WORLD_CAMERA_KEY: u32 = 89;
/// `StartPath()`.
pub const EE_SCENARIO_WORLD_CAMERA_START: u32 = 90;
/// The move's end (a 0) or start (1) point = (b, c, d)'s f32s.
pub const EE_SCENARIO_WORLD_CAMERA_POINT: u32 = 91;
/// `SetMode(3)`.
pub const EE_SCENARIO_WORLD_CAMERA_MODE: u32 = 92;
/// `MoveTo(end unit a, the end point, c & 1, (float)d, start unit b, the start point, c & 2, e's f32, 0, 0)` (no
/// unit for -1).
pub const EE_SCENARIO_WORLD_CAMERA_MOVE: u32 = 93;

/// The unit values of [`EE_SCENARIO_WORLD_UNIT`].
pub const EE_SCENARIO_UV_EXISTS: i64 = 0;
/// `mComplexUnit != NULL`.
pub const EE_SCENARIO_UV_COMPLEX: i64 = 1;
pub const EE_SCENARIO_UV_HIT_POINTS: i64 = 2;
/// `GetHitPoints() < GetStats()->mMaxHitPoints`.
pub const EE_SCENARIO_UV_DAMAGED: i64 = 3;
pub const EE_SCENARIO_UV_TILE_X: i64 = 4;
pub const EE_SCENARIO_UV_TILE_Y: i64 = 5;
/// `mTile != NULL`.
pub const EE_SCENARIO_UV_ON_TILE: i64 = 6;
/// `mPlayer->mPlayerID`.
pub const EE_SCENARIO_UV_PLAYER: i64 = 7;
/// `gWorld.GetPlayerIndex(GetPlayer())`.
pub const EE_SCENARIO_UV_PLAYER_INDEX: i64 = 8;
/// The class's unit type.
pub const EE_SCENARIO_UV_TYPE: i64 = 9;
/// The class's category.
pub const EE_SCENARIO_UV_CATEGORY: i64 = 10;
/// The complex unit's `GetCommandType()`.
pub const EE_SCENARIO_UV_COMMAND: i64 = 11;
pub const EE_SCENARIO_UV_DISABLED: i64 = 12;
pub const EE_SCENARIO_UV_COMMANDABLE: i64 = 13;
/// The class's `mIsDeletable`.
pub const EE_SCENARIO_UV_CLASS_DELETABLE: i64 = 14;
/// The class's `mIsWall`.
pub const EE_SCENARIO_UV_WALL: i64 = 15;
/// `GetGoal() != NULL`.
pub const EE_SCENARIO_UV_HAS_GOAL: i64 = 16;
/// The complex unit's `GetMaxAngularVelocity() > 0`.
pub const EE_SCENARIO_UV_TURNS: i64 = 17;
/// The class's `GetIsTree()`.
pub const EE_SCENARIO_UV_TREE: i64 = 18;
/// `GetUnitID()`.
pub const EE_SCENARIO_UV_ID: i64 = 19;
/// The container's id, -1 for none.
pub const EE_SCENARIO_UV_CONTAINER: i64 = 20;
/// The container's unit type.
pub const EE_SCENARIO_UV_CONTAINER_TYPE: i64 = 21;
/// The class's `mDomains & kMovementAir`.
pub const EE_SCENARIO_UV_AIR: i64 = 22;
/// The class's `IsLandBuilding()`.
pub const EE_SCENARIO_UV_LAND_BUILDING: i64 = 23;
/// The class's `IsTransport()`.
pub const EE_SCENARIO_UV_TRANSPORT: i64 = 24;
/// The class's `IsDropSite()`.
pub const EE_SCENARIO_UV_DROP_SITE: i64 = 25;
/// The class's `mGarrisonCapacity`.
pub const EE_SCENARIO_UV_GARRISON: i64 = 26;
/// `mIsDetachedFromAI`.
pub const EE_SCENARIO_UV_DETACHED: i64 = 27;
/// `GetNumCargoUnits()`.
pub const EE_SCENARIO_UV_CARGO: i64 = 28;

/// [`EE_SCENARIO_WORLD_CLASS`]'s bits.
pub const EE_SCENARIO_CLASS_EXISTS: i64 = 1;
/// `GetDomains() & kMovementAir`.
pub const EE_SCENARIO_CLASS_AIR: i64 = 2;
/// `GetDomains() & kMovementSea`.
pub const EE_SCENARIO_CLASS_SEA: i64 = 4;
pub const EE_SCENARIO_CLASS_PROJECTILE: i64 = 8;
pub const EE_SCENARIO_CLASS_LAND_BUILDING: i64 = 16;
/// `GetAttack() > 0`.
pub const EE_SCENARIO_CLASS_ATTACKS: i64 = 32;
/// `GetMaxVelocity() > 0`.
pub const EE_SCENARIO_CLASS_MOVES: i64 = 64;
pub const EE_SCENARIO_CLASS_FLIES_OVER: i64 = 128;
/// A category Set Favorite Units never takes as the land unit (the buildings, ships, aircraft, missiles...).
pub const EE_SCENARIO_CLASS_NOT_LAND: i64 = 256;

/// [`EE_SCENARIO_WORLD_GFX_BEGIN`]'s bits: rain or snow (weather over the tiles).
pub const EE_SCENARIO_GFX_WEATHER: i64 = 1;
/// Tracers, shells, winter breath or a muzzle blast (between a complex unit and the next unit).
pub const EE_SCENARIO_GFX_PAIR: i64 = 2;
/// A marching line (between any unit and the next).
pub const EE_SCENARIO_GFX_MARCHING: i64 = 4;

// the actions, categories and unit types the effects test (ee/EEUnitAction.h, ee/EEUnitCategory.h,
// ee/EEUnitTypes.h)
pub(crate) const EE_SCENARIO_K_ACTION_IDLE: i64 = 5000;
pub(crate) const EE_SCENARIO_K_ACTION_CIRCLE: i64 = 1008;
pub(crate) const EE_SCENARIO_K_ACTION_CLOSING: i64 = 15003;
pub(crate) const EE_SCENARIO_K_ACTION_PREPARE_TO_MOVE: i64 = 8000;
pub(crate) const EE_SCENARIO_K_ACTION_RESOURCES_ROTTING: i64 = 6001;
pub(crate) const EE_SCENARIO_K_ACTION_UNIT_ROTTING: i64 = 6002;
pub(crate) const EE_SCENARIO_K_ACTION_DEAD: i64 = 6003;
pub(crate) const EE_SCENARIO_K_ACTION_RESOURCES_DEPLETING: i64 = 6004;
pub(crate) const EE_SCENARIO_K_CATEGORY_AIRPLANE: i64 = 3;
pub(crate) const EE_SCENARIO_K_CATEGORY_GATE: i64 = 0x19;
pub(crate) const EE_SCENARIO_K_CATEGORY_NAVAL_CITIZEN: i64 = 0x16;
pub(crate) const EE_SCENARIO_K_CATEGORY_PLANET_SPACESHIP: i64 = 0x22;
pub(crate) const EE_SCENARIO_K_TYPE_FARM: i64 = 0x2a;
pub(crate) const EE_SCENARIO_K_TYPE_CAMERA_OBJECT: i64 = 0x167;
pub(crate) const EE_SCENARIO_K_TYPE_AIRPORT: i64 = 0x42;
pub(crate) const EE_SCENARIO_K_TYPE_DOCK: i64 = 0x24;
pub(crate) const EE_SCENARIO_K_TYPE_NAVY_YARD: i64 = 0x41;
pub(crate) const EE_SCENARIO_K_TYPE_PATH_POINT: i64 = 0xdf;

/// `kSMUnitAttributeShockArmorPlus`: the first class attribute.
const ATTRIBUTE_CLASS_FIRST: c_ulong = 20;
/// `(unsigned long)-1`.
const NONE: c_ulong = c_ulong::MAX;

/// A world call of up to six arguments.
pub(crate) fn w<H: Host>(h: &mut H, op: u32, args: &[i64]) -> i64 {
    let mut a = [0i64; 6];
    a[..args.len()].copy_from_slice(args);
    h.world(op, a)
}

/// Unit `id`'s value `field`.
pub(crate) fn unit<H: Host>(h: &mut H, id: i64, field: i64) -> i64 {
    w(h, EE_SCENARIO_WORLD_UNIT, &[id, field])
}

/// Whether unit `id` exists (`gWorld.GetUnit(id) != NULL`).
pub(crate) fn exists<H: Host>(h: &mut H, id: i64) -> bool {
    unit(h, id, EE_SCENARIO_UV_EXISTS) != 0
}

/// Whether unit `id` exists with a complex unit and hit points above 0 (the effects' "living unit").
pub(crate) fn alive_complex<H: Host>(h: &mut H, id: i64) -> bool {
    exists(h, id) && unit(h, id, EE_SCENARIO_UV_COMPLEX) != 0 && unit(h, id, EE_SCENARIO_UV_HIT_POINTS) > 0
}

/// Unit `id`'s tile as (x, y).
pub(crate) fn tile<H: Host>(h: &mut H, id: i64) -> (i64, i64) {
    (unit(h, id, EE_SCENARIO_UV_TILE_X), unit(h, id, EE_SCENARIO_UV_TILE_Y))
}

/// An aircraft's category (an airplane or a planet spaceship).
pub(crate) fn flies(category: i64) -> bool {
    category == EE_SCENARIO_K_CATEGORY_AIRPLANE || category == EE_SCENARIO_K_CATEGORY_PLANET_SPACESHIP
}

/// The object's unit count (`mUnits.size()`).
pub(crate) fn count<H: Host>(h: &mut H, object: c_ulong) -> u32 {
    h.read(EE_SCENARIO_READ_OBJECT_UNIT_COUNT, object as i32, 0) as u32
}

/// The object's unit `i` (`mUnits[i]`).
pub(crate) fn unit_at<H: Host>(h: &mut H, object: c_ulong, i: u32) -> i64 {
    h.read(EE_SCENARIO_READ_OBJECT_UNIT_AT, object as i32, i as i32)
}

/// `GetScenarioObject(object)->Evaluate(false)` (-1 for no object).
pub(crate) fn evaluate<H: Host>(h: &mut H, object: c_ulong) -> i64 {
    w(h, EE_SCENARIO_WORLD_OBJECT_EVALUATE, &[object as i64])
}

/// `GetArea(area)`: `None` for no area, else (unset, off the map).
pub(crate) fn area_state<H: Host>(h: &mut H, area: c_ulong) -> Option<(bool, bool)> {
    let s = w(h, EE_SCENARIO_WORLD_AREA_STATE, &[area as i64]);
    (s >= 0).then_some((s & 1 != 0, s & 2 != 0))
}

/// Whether area `area` exists, is set and is on the map.
pub(crate) fn area_usable<H: Host>(h: &mut H, area: c_ulong) -> bool {
    area_state(h, area) == Some((false, false))
}

pub(crate) fn unpack(v: i64) -> (i64, i64) {
    (v as u32 as i32 as i64, (v >> 32) as u32 as i32 as i64)
}

fn bits(v: f32) -> i64 {
    v.to_bits() as i64
}

/// The chosen players of a choice that takes them (`ChoosePlayer(kind, &players, true)`).
pub(crate) fn players<H: Host>(h: &mut H, kind: c_ulong) -> Vec<i32> {
    choose_player_recorded(h, kind as c_long as i32, true, true).1
}

/// `EESMEffect::Execute`'s body: the effect `e` (its numbers as on entry; `cinematic` whether it runs in a
/// cinematic).
pub fn execute<H: Host>(h: &mut H, e: &EeScenarioEffectFields, cinematic: bool) {
    match e.mType {
        1 => match e.mObjectAction {
            0 => {
                w(h, EE_SCENARIO_WORLD_CREATE_UNITS, &[]);
            }
            1 => freeze_units(h, e, true),
            2 => freeze_units(h, e, false),
            3 => kill_units(h, e, true),
            4 => kill_units(h, e, false),
            5 => crate::task::task_units_to_object(h, e),
            6 => crate::task::task_units_to_area(h, e),
            7 => set_units_stance(h, e),
            9 => set_units_owner(h, e),
            8 | 12 => change_attribute(h, e),
            10 => set_class_name(h, e),
            11 => face_object(h, e),
            _ => {}
        },
        2 => player_effect(h, e),
        3 => {
            let t = e.mTrigger as i64;
            let set = |h: &mut H, which: i64, value: i64| {
                w(h, EE_SCENARIO_WORLD_TRIGGER_SET, &[t, which, value]);
            };
            match e.mTriggerState {
                0 => {
                    set(h, 0, 0);
                    set(h, 1, 1);
                }
                1 => set(h, 1, 0),
                2 => set(h, 2, 1),
                3 => set(h, 2, 0),
                _ => {}
            }
        }
        4 => media_effect(h, e, cinematic),
        5 => game_effect(h, e),
        6 => match e.mAIAction {
            0 => crate::effect_ai::modify_ai_military_target(h, e),
            1 => crate::effect_ai::create_ai_military_group(h, e),
            2 => crate::effect_ai::build_object(h, e),
            4 => set_ai_toggle(h, e),
            5 => set_ai_variable(h, e),
            6 => set_favorite_units(h, e),
            _ => {} // 3: MaintainObject does nothing
        },
        _ => {}
    }
}

fn player_effect<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    let list = players(h, e.mPlayer);
    for &p in &list {
        let p64 = p as i64;
        let exists = |h: &mut H| h.read(EE_SCENARIO_READ_PLAYER_EXISTS, p, 0) != 0;
        match e.mPlayerAction {
            1 => {
                if w(h, EE_SCENARIO_WORLD_CURRENT_PLAYER, &[]) == p64 {
                    script_camera(h, e);
                }
            }
            0 | 2 => {
                if p != 0 && exists(h) {
                    if w(h, EE_SCENARIO_WORLD_PLAYER_OBSERVER, &[p64]) == 0 {
                        w(h, EE_SCENARIO_WORLD_PLAYER_RESULT, &[p64, (e.mPlayerAction == 0) as i64]);
                    }
                    if h.read(EE_SCENARIO_READ_PLAYER_COMPUTER, p, 0) != 0 {
                        w(h, EE_SCENARIO_WORLD_COMPUTER_SET_ACTIVE, &[p64, 0]);
                    }
                }
            }
            3 => {
                if exists(h) {
                    let others = players(h, e.mDiplomacyPlayer);
                    for &o in &others {
                        if o == p {
                            continue;
                        }
                        let id = h.read(EE_SCENARIO_READ_PLAYER_ID, p, 0);
                        w(h, EE_SCENARIO_WORLD_SET_STANCE, &[id, o as i64, e.mDiplomacy as i64]);
                        if e.mDiplomacy != 1 {
                            w(h, EE_SCENARIO_WORLD_NOTICE_TARGET, &[p64, o as i64]);
                        }
                    }
                }
            }
            7 => {
                if p64 == w(h, EE_SCENARIO_WORLD_CURRENT_PLAYER, &[]) {
                    w(h, EE_SCENARIO_WORLD_FADE, &[1]);
                }
            }
            8 => {
                if w(h, EE_SCENARIO_WORLD_IN_CINEMATIC, &[]) != 0 && p64 == w(h, EE_SCENARIO_WORLD_CURRENT_PLAYER, &[])
                {
                    w(h, EE_SCENARIO_WORLD_FADE, &[0]);
                }
            }
            4 => h.write(
                EE_SCENARIO_WRITE_SET_ATTRIBUTE,
                p as c_long,
                e.mPlayerAttribute as i64,
                e.mPlayerAttributeMode as i64,
                e.mPlayerAttributeAmount as i64,
            ),
            5 => {
                if exists(h) {
                    let op =
                        if e.mIsAddTechnology { EE_SCENARIO_WORLD_TECH_ADD } else { EE_SCENARIO_WORLD_TECH_REMOVE };
                    w(h, op, &[p64]);
                    w(h, EE_SCENARIO_WORLD_ADD_PENDING, &[]);
                }
            }
            9 | 10 => {
                if exists(h) {
                    w(h, EE_SCENARIO_WORLD_SEES_ALL, &[p64, (e.mPlayerAction == 9) as i64]);
                }
            }
            6 => {
                if exists(h) {
                    w(h, EE_SCENARIO_WORLD_CIV_POINTS, &[p64, e.mCivPoints as i64]);
                }
            }
            11 => {
                if exists(h) {
                    if let Some((x, y)) = flare_tile(h, e) {
                        w(h, EE_SCENARIO_WORLD_FLARE, &[p64, x, y]);
                    }
                }
            }
            _ => {}
        }
    }
}

/// The mini map flare's tile: the location area's centre (a map tile) or the location object's first existing unit's
/// (or its container's); `None` for no flare. (The original read through a missing area; none is skipped.)
fn flare_tile<H: Host>(h: &mut H, e: &EeScenarioEffectFields) -> Option<(i64, i64)> {
    if e.mUsesLocationArea {
        let (unset, off) = area_state(h, e.mLocationArea)?;
        if off || unset {
            return None;
        }
        let (x, y) = unpack(w(h, EE_SCENARIO_WORLD_AREA_CENTER, &[e.mLocationArea as i64]));
        return (h.read(EE_SCENARIO_READ_MAP_TILE_EXISTS, x as i32, y as i32) != 0).then_some((x, y));
    }
    evaluate(h, e.mLocationObject);
    let mut found = None;
    let mut i = 0;
    while found.is_none() && i < count(h, e.mLocationObject) {
        let id = unit_at(h, e.mLocationObject, i);
        if exists(h, id) {
            found = Some(id);
        }
        i += 1;
    }
    let packed = w(h, EE_SCENARIO_WORLD_FLARE_UNIT_TILE, &[found?]);
    (packed != -1).then(|| unpack(packed))
}

fn media_effect<H: Host>(h: &mut H, e: &EeScenarioEffectFields, cinematic: bool) {
    match e.mMediaAction {
        0 => {
            w(h, EE_SCENARIO_WORLD_PLAY_SOUND, &[]);
        }
        1 => {
            w(h, EE_SCENARIO_WORLD_CHANGE_TEXT, &[e.mTextObject as i64, e.mIsClearText as i64]);
        }
        2 => {
            w(h, EE_SCENARIO_WORLD_EXPAND_MESSAGE, &[]);
            let list = players(h, e.mMessagePlayer);
            for &p in &list {
                if p as i64 == w(h, EE_SCENARIO_WORLD_CURRENT_PLAYER, &[]) {
                    w(h, EE_SCENARIO_WORLD_DIALOGUE, &[cinematic as i64]);
                } else {
                    w(h, EE_SCENARIO_WORLD_DISPLAY_EMPTY, &[]);
                }
            }
        }
        3 => {
            w(h, EE_SCENARIO_WORLD_EXPAND_MESSAGE, &[]);
            let list = players(h, e.mMessagePlayer);
            for &p in &list {
                if p as i64 != w(h, EE_SCENARIO_WORLD_CURRENT_PLAYER, &[]) {
                    continue;
                }
                let op = if e.mDuration > 0 { EE_SCENARIO_WORLD_CHAT_LOG } else { EE_SCENARIO_WORLD_CHAT_BOX };
                w(h, op, &[]);
            }
        }
        _ => {}
    }
}

/// `1.0f / 24.0f` (the original's .rdata float).
const HOUR_FRACTION: f32 = 1.0 / 24.0;

fn game_effect<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    match e.mGameAction {
        0 => {
            let hour = (e.mDaylightHour.wrapping_add(18) % 24) as f32;
            w(h, EE_SCENARIO_WORLD_SET_TIME_OF_DAY, &[bits(hour * HOUR_FRACTION)]);
        }
        1 => {
            w(h, EE_SCENARIO_WORLD_TIME_OF_DAY_RUNS, &[(e.mCycleTime > 0) as i64]);
        }
        2 => calamity(h, e),
        3 => graphic_effect(h, e),
        4 => {
            w(h, EE_SCENARIO_WORLD_START_CINEMATIC, &[]);
            // the original rounds x * 0.1f, then adds 0.1f
            let mut speed = e.mDaylightHour as f32 * 0.1f32;
            speed += 0.1f32;
            w(h, EE_SCENARIO_WORLD_SET_GAME_SPEED, &[bits(speed)]);
        }
        5 => {
            if w(h, EE_SCENARIO_WORLD_DIALOG_IDLE, &[]) != 0 {
                w(h, EE_SCENARIO_WORLD_END_CINEMATIC, &[]);
            } else {
                w(h, EE_SCENARIO_WORLD_TIMED_NOT_RUN, &[]);
            }
        }
        6 => {
            w(h, EE_SCENARIO_WORLD_WEATHER, &[e.mIsOn as i64]);
        }
        _ => {}
    }
}

/// The Calamity game action: a global calamity, one at a random tile of the place area or at the place object's
/// units (location kinds), or on the place object's units or every unit in the place area (unit kinds).
fn calamity<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    let mut kind = e.mPlacedEffect as i64;
    if kind == 0 {
        kind = h.read(EE_SCENARIO_READ_RANDOM, 1, 0x21) as c_ulong as i64;
    }
    if w(h, EE_SCENARIO_WORLD_CAL_CLASS, &[kind]) == 0 {
        return;
    }
    let ty = w(h, EE_SCENARIO_WORLD_CAL_TYPE, &[kind]);
    let can_cast = |h: &mut H, x: i64, y: i64, u: i64| w(h, EE_SCENARIO_WORLD_CAL_CAN_CAST, &[x, y, u]) != 0;
    let object = e.mPlaceObject;
    let object_exists = |h: &mut H| h.read(crate::host::EE_SCENARIO_READ_OBJECT_EXISTS, object as i32, 0) != 0;
    match ty {
        1 => {
            if can_cast(h, 0, 0, -1) {
                w(h, EE_SCENARIO_WORLD_CAL_GLOBAL, &[kind]);
            }
        }
        2 => {
            let area = area_state(h, e.mPlaceArea);
            if e.mUsesPlaceArea && area.is_some() {
                if area == Some((false, false)) {
                    let mut tiles = h.tiles(EE_SCENARIO_TILES_AREA, e.mPlaceArea as i32, 0, 0);
                    shuffle(h, &mut tiles);
                    if let Some(&(x, y)) = tiles.first() {
                        if can_cast(h, x as i64, y as i64, -1) {
                            w(h, EE_SCENARIO_WORLD_CAL_LOCATION, &[kind, x as i64, y as i64]);
                        }
                    }
                }
            } else if !e.mUsesPlaceArea && object_exists(h) && evaluate(h, object) == 1 {
                let mut i = 0;
                while i < count(h, object) {
                    let id = unit_at(h, object, i);
                    if exists(h, id) {
                        let (x, y) = tile(h, id);
                        if can_cast(h, x, y, id) {
                            w(h, EE_SCENARIO_WORLD_CAL_LOCATION, &[kind, x, y]);
                        }
                    }
                    i += 1;
                }
            }
        }
        3 => {
            if object_exists(h) && !e.mUsesPlaceArea && evaluate(h, object) == 1 {
                let mut i = 0;
                while i < count(h, object) {
                    let id = unit_at(h, object, i);
                    if exists(h, id) {
                        let (x, y) = tile(h, id);
                        if can_cast(h, x, y, id) {
                            let target = unit_at(h, object, i);
                            w(h, EE_SCENARIO_WORLD_CAL_UNIT, &[kind, target]);
                        }
                    }
                    i += 1;
                }
            }
            if e.mUsesPlaceArea && area_usable(h, e.mPlaceArea) {
                let tiles = h.tiles(EE_SCENARIO_TILES_AREA, e.mPlaceArea as i32, 0, 0);
                for &(x, y) in &tiles {
                    w(h, EE_SCENARIO_WORLD_CAL_TILE_UNITS, &[kind, x as i64, y as i64]);
                }
            }
        }
        _ => {}
    }
}

/// The Graphic Effect game action: at a random tile of the place area (weather over all of it), or on (or between)
/// the place object's units.
fn graphic_effect<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    let kind = w(h, EE_SCENARIO_WORLD_GFX_BEGIN, &[e.mPlacedEffect as i64]);
    if kind < 0 {
        return;
    }
    if e.mUsesPlaceArea {
        if area_usable(h, e.mPlaceArea) {
            let mut tiles = h.tiles(EE_SCENARIO_TILES_AREA, e.mPlaceArea as i32, 0, 0);
            if kind & EE_SCENARIO_GFX_WEATHER != 0 {
                let flat: Vec<i32> = tiles.iter().flat_map(|&(x, y)| [x, y]).collect();
                w(h, EE_SCENARIO_WORLD_GFX_WEATHER, &[flat.as_ptr() as i64, tiles.len() as i64]);
            } else {
                shuffle(h, &mut tiles);
                if let Some(&(x, y)) = tiles.first() {
                    if h.read(EE_SCENARIO_READ_MAP_TILE_EXISTS, x, y) != 0 {
                        w(h, EE_SCENARIO_WORLD_GFX_PLACED, &[x as i64, y as i64]);
                    }
                }
            }
        }
    } else {
        let object = e.mPlaceObject;
        if h.read(crate::host::EE_SCENARIO_READ_OBJECT_EXISTS, object as i32, 0) != 0 && evaluate(h, object) == 1 {
            let mut i = 0;
            while i < count(h, object) {
                let id = unit_at(h, object, i);
                if !exists(h, id) {
                    i += 1;
                    continue;
                }
                let next = if i + 1 < count(h, object) {
                    let n = unit_at(h, object, i + 1);
                    exists(h, n).then_some(n)
                } else {
                    None
                };
                let pair = kind & EE_SCENARIO_GFX_PAIR != 0 && unit(h, id, EE_SCENARIO_UV_COMPLEX) != 0;
                match next {
                    Some(n) if pair || kind & EE_SCENARIO_GFX_MARCHING != 0 => {
                        w(h, EE_SCENARIO_WORLD_GFX_BETWEEN, &[id, n]);
                    }
                    _ => {
                        w(h, EE_SCENARIO_WORLD_GFX_UNIT, &[id]);
                    }
                }
                i += 1;
            }
        }
    }
    w(h, EE_SCENARIO_WORLD_GFX_END, &[]);
}

/// Evaluates the object when the original did (a pick-on-map object only when dynamic or empty).
fn evaluate_for_units<H: Host>(h: &mut H, object: c_ulong) -> bool {
    let Some(f) = h.object(object as i32) else { return false };
    if !f.mIsSelectOnMap && (f.mIsDynamic || count(h, object) == 0) {
        evaluate(h, object);
    }
    true
}

/// Freeze (stopped, aircraft circle, gates close; disabled, not commandable or deletable) or Unfreeze the object's
/// living units.
fn freeze_units<H: Host>(h: &mut H, e: &EeScenarioEffectFields, freeze: bool) {
    if !evaluate_for_units(h, e.mObject) {
        return;
    }
    let n = count(h, e.mObject);
    for i in 0..n {
        let id = unit_at(h, e.mObject, i);
        if !alive_complex(h, id) {
            continue;
        }
        if freeze {
            w(h, EE_SCENARIO_WORLD_CLEAR_GOALS, &[id, 0]);
            let category = unit(h, id, EE_SCENARIO_UV_CATEGORY);
            if flies(category) && unit(h, id, EE_SCENARIO_UV_COMMAND) != EE_SCENARIO_K_ACTION_IDLE {
                w(h, EE_SCENARIO_WORLD_ACTION, &[id, EE_SCENARIO_K_ACTION_CIRCLE]);
            } else if category == EE_SCENARIO_K_CATEGORY_GATE {
                w(h, EE_SCENARIO_WORLD_ACTION, &[id, EE_SCENARIO_K_ACTION_CLOSING]);
            } else if unit(h, id, EE_SCENARIO_UV_WALL) == 0 {
                w(h, EE_SCENARIO_WORLD_ACTION, &[id, EE_SCENARIO_K_ACTION_IDLE]);
            }
            w(h, EE_SCENARIO_WORLD_SET_DELETABLE, &[id, 0]);
        } else {
            let deletable = unit(h, id, EE_SCENARIO_UV_CLASS_DELETABLE);
            w(h, EE_SCENARIO_WORLD_SET_DELETABLE, &[id, deletable]);
        }
        w(h, EE_SCENARIO_WORLD_SET_DISABLED, &[id, freeze as i64]);
        w(h, EE_SCENARIO_WORLD_SET_COMMANDABLE, &[id, !freeze as i64]);
    }
}

/// Kill (or Remove) the object's living units.
fn kill_units<H: Host>(h: &mut H, e: &EeScenarioEffectFields, kill: bool) {
    if !evaluate_for_units(h, e.mObject) {
        return;
    }
    let n = count(h, e.mObject);
    for i in 0..n {
        let id = unit_at(h, e.mObject, i);
        if exists(h, id) && unit(h, id, EE_SCENARIO_UV_HIT_POINTS) > 0 {
            w(h, EE_SCENARIO_WORLD_KILL, &[id, kill as i64]);
        }
    }
}

/// Set Units Owner: the object's living units (farms last, the newest first) converted, each to a player picked from
/// the new-owner setting (nature never).
fn set_units_owner<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    if h.object(e.mObject as i32).is_none() {
        return; // (the original read through a missing object)
    }
    evaluate(h, e.mObject);
    let n = count(h, e.mObject);
    let mut units = Vec::new();
    for i in 0..n {
        let id = unit_at(h, e.mObject, i);
        if exists(h, id) && unit(h, id, EE_SCENARIO_UV_HIT_POINTS) > 0 {
            let farm = unit(h, id, EE_SCENARIO_UV_TYPE) == EE_SCENARIO_K_TYPE_FARM;
            units.push((farm, unit(h, id, EE_SCENARIO_UV_ID) as u32, id));
        }
    }
    // EESMUnitPriorityLess: non-farms first, then the higher id first (a total order: ids are unique)
    units.sort_by(|a, b| a.0.cmp(&b.0).then(b.1.cmp(&a.1)));
    for &(_, _, id) in &units {
        if !exists(h, id) || unit(h, id, EE_SCENARIO_UV_HIT_POINTS) <= 0 {
            continue;
        }
        let player = choose_player_recorded(h, e.mNewOwner as c_long as i32, true, false).0;
        if player != -1 && player != 0 {
            w(h, EE_SCENARIO_WORLD_CONVERT, &[id, player as i64]);
        }
    }
}

/// `0.017453292f`: a degree in radians.
const DEGREE: f32 = 0.017_453_292;

/// Face Object: the object's idle living units turned to `mAttributeAmount` degrees.
fn face_object<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    if h.object(e.mObject as i32).is_none() {
        return; // (the original read through a missing object)
    }
    evaluate(h, e.mObject);
    let n = count(h, e.mObject);
    for i in 0..n {
        let id = unit_at(h, e.mObject, i);
        if !alive_complex(h, id)
            || (unit(h, id, EE_SCENARIO_UV_HAS_GOAL) != 0
                && unit(h, id, EE_SCENARIO_UV_COMMAND) != EE_SCENARIO_K_ACTION_IDLE)
            || unit(h, id, EE_SCENARIO_UV_TURNS) == 0
        {
            continue;
        }
        let yaw = bits(e.mAttributeAmount as f32 * DEGREE);
        if w(h, EE_SCENARIO_WORLD_FACE_TEST, &[id, yaw]) == 0 {
            continue;
        }
        let op = if e.mUsesPercent { EE_SCENARIO_WORLD_FACE_SET } else { EE_SCENARIO_WORLD_FACE_GOAL };
        w(h, op, &[id, yaw]);
    }
}

/// Whether the object selects its units beyond its class and owner (any check or a parent): then it is evaluated.
fn selects(f: &crate::fields::EeScenarioObjectFields) -> bool {
    f.mIsSelectOnMap
        || f.mInAreaCheck != 0
        || f.mHasAttributeCheck != 0
        || f.mNearCheck != 0
        || f.mSelectedByCheck != 0
        || f.mHasStateCheck != 0
        || f.mVisibleByCheck != 0
        || f.mParent != 0
}

/// Class-Name: the object's living units take the new name, or with no units each owner's upgrade of a picked type.
fn set_class_name<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    let Some(f) = h.object(e.mObject as i32) else { return };
    if selects(&f) {
        evaluate(h, e.mObject);
    }
    let n = count(h, e.mObject);
    if n > 0 {
        for i in 0..n {
            let id = unit_at(h, e.mObject, i);
            if exists(h, id) && unit(h, id, EE_SCENARIO_UV_HIT_POINTS) > 0 {
                w(h, EE_SCENARIO_WORLD_RENAME_UNIT, &[id]);
            }
        }
    } else {
        let list = players(h, f.mOwner);
        for &p in &list {
            let class = w(h, EE_SCENARIO_WORLD_OBJECT_PICK, &[e.mObject as i64]);
            if class >= 0 {
                w(h, EE_SCENARIO_WORLD_RENAME_CLASS, &[p as i64, class]);
            }
        }
    }
    w(h, EE_SCENARIO_WORLD_REFRESH_PANEL, &[]);
}

/// Stance: explore, stop, lock or unlock (gates), patrol the town, or a behaviour.
fn set_units_stance<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    if h.object(e.mObject as i32).is_none() {
        return;
    }
    evaluate(h, e.mObject);
    let n = count(h, e.mObject);
    for i in 0..n {
        let id = unit_at(h, e.mObject, i);
        if !alive_complex(h, id) {
            continue;
        }
        match e.mStance {
            0 => {
                w(h, EE_SCENARIO_WORLD_STANCE_EXPLORE, &[id]);
            }
            1 => {
                w(h, EE_SCENARIO_WORLD_CLEAR_QUEUE, &[id, 0]);
                let action = if flies(unit(h, id, EE_SCENARIO_UV_CATEGORY)) { 0x3f0 } else { 0x1388 };
                w(h, EE_SCENARIO_WORLD_ACTION, &[id, action]);
            }
            2 | 3 => {
                w(h, EE_SCENARIO_WORLD_LOCK, &[id, (e.mStance == 2) as i64]);
            }
            4 => {
                w(h, EE_SCENARIO_WORLD_STANCE_TOWN_PATROL, &[id]);
            }
            5..=10 => {
                w(h, EE_SCENARIO_WORLD_BEHAVIOR, &[id, e.mStance as i64 - 5]);
            }
            _ => {}
        }
    }
}

/// Unit-Attribute / Class-Attribute: the object's units' attribute, or a class attribute of each (type, player) the
/// object covers; without an object, the effect's own unit or the player's class upgrade.
fn change_attribute<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    let attr = e.mAttribute as i64;
    let percent = e.mUsesPercent as i64;
    let amount = e.mAttributeAmount as i64;
    let mode = e.mAttributeMode as i64;
    let object = h.object(e.mObject as i32);
    match object {
        Some(f) if e.mAttributeUnit == NONE && e.mAttributeUnitType == 0 => {
            if e.mAttribute < ATTRIBUTE_CLASS_FIRST {
                evaluate(h, e.mObject);
                let n = count(h, e.mObject);
                for i in 0..n {
                    let id = unit_at(h, e.mObject, i);
                    if exists(h, id)
                        && (!f.mIsSelectOnMap
                            || unit(h, id, EE_SCENARIO_UV_HIT_POINTS) > 0
                            || unit(h, id, EE_SCENARIO_UV_TREE) != 0)
                    {
                        w(h, EE_SCENARIO_WORLD_CHANGE_UNIT, &[id, attr, percent, amount, mode]);
                    }
                }
                return;
            }
            let mut targets: Vec<(i64, i64)> = Vec::new();
            if !selects(&f) {
                let list = players(h, f.mOwner);
                for &p in &list {
                    let class = w(h, EE_SCENARIO_WORLD_OBJECT_PICK, &[e.mObject as i64]);
                    if class < 0 {
                        continue;
                    }
                    if !targets.contains(&(class, p as i64)) {
                        targets.push((class, p as i64));
                    }
                }
            } else {
                evaluate(h, e.mObject);
                let n = count(h, e.mObject);
                for i in 0..n {
                    let id = unit_at(h, e.mObject, i);
                    if !exists(h, id) || unit(h, id, EE_SCENARIO_UV_HIT_POINTS) <= 0 {
                        continue;
                    }
                    let target = (unit(h, id, EE_SCENARIO_UV_TYPE), unit(h, id, EE_SCENARIO_UV_PLAYER));
                    if !targets.contains(&target) {
                        targets.push(target);
                    }
                }
            }
            for &(class, p) in &targets {
                modify_unit_attribute(h, p, class, e.mAttribute, e.mUsesPercent, e.mAttributeAmount as c_long, e.mAttributeMode as c_long);
            }
        }
        _ => {
            if e.mAttribute >= ATTRIBUTE_CLASS_FIRST {
                // the effect's own player (none below 0) and its upgrade of the unit type (none for type 0)
                let p = e.mAttributePlayer as i64;
                let class = if p >= 0 && e.mAttributeUnitType != 0 { e.mAttributeUnitType as i64 } else { -1 };
                modify_unit_attribute(h, p, class, e.mAttribute, e.mUsesPercent, e.mAttributeAmount as c_long, e.mAttributeMode as c_long);
            } else {
                let id = if e.mAttributeUnit != NONE { e.mAttributeUnit as i64 } else { -1 };
                w(h, EE_SCENARIO_WORLD_CHANGE_UNIT, &[id, attr, percent, amount, mode]);
            }
        }
    }
}

/// The chosen players that are computer players (`!IsHuman()`), in the choice's order.
fn computer_players<H: Host>(h: &mut H, kind: c_ulong) -> Vec<i32> {
    let list = players(h, kind);
    let mut out = Vec::new();
    for &p in &list {
        if h.read(EE_SCENARIO_READ_PLAYER_EXISTS, p, 0) != 0
            && w(h, EE_SCENARIO_WORLD_PLAYER_IS_HUMAN, &[p as i64]) == 0
        {
            out.push(p);
        }
    }
    out
}

fn set_ai_toggle<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    let list = players(h, e.mPlayer);
    for &p in &list {
        if h.read(EE_SCENARIO_READ_PLAYER_EXISTS, p, 0) == 0
            || w(h, EE_SCENARIO_WORLD_PLAYER_IS_HUMAN, &[p as i64]) != 0
        {
            continue;
        }
        if e.mAIToggle < 8 {
            w(h, EE_SCENARIO_WORLD_AI_TOGGLE, &[p as i64, e.mAIToggle as i64, e.mIsOn as i64]);
        }
    }
}

/// Set AI Variable: the stores of [`crate::aivar`] on each chosen computer player; a deferred behaviour also queues a
/// copy of the effect for that player (player value id + 6).
fn set_ai_variable<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    let mut writes: Vec<EeScenarioAiWrite> = Vec::new();
    let deferred = ai_variable(e, &mut writes);
    let list = players(h, e.mPlayer);
    for &p in &list {
        if h.read(EE_SCENARIO_READ_PLAYER_EXISTS, p, 0) == 0
            || w(h, EE_SCENARIO_WORLD_PLAYER_IS_HUMAN, &[p as i64]) != 0
        {
            continue;
        }
        for write in &writes {
            w(h, EE_SCENARIO_WORLD_AI_STORE, &[p as i64, write as *const EeScenarioAiWrite as i64]);
        }
        if deferred {
            let id = h.read(EE_SCENARIO_READ_PLAYER_ID, p, 0);
            w(h, EE_SCENARIO_WORLD_ADD_PENDING_PLAYER, &[id + 6]);
        }
    }
}

/// Set Favorite Units: the air (`mObject`), land (`mIntValue1`) and sea (`mIntValue2`) unit types, each only when the
/// class is a moving unit of that kind (air and sea ones must attack).
fn set_favorite_units<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    for p in computer_players_lazy(h, e) {
        let air = w(h, EE_SCENARIO_WORLD_CLASS, &[e.mObject as i64]);
        if air & EE_SCENARIO_CLASS_EXISTS != 0
            && air & EE_SCENARIO_CLASS_AIR != 0
            && air & (EE_SCENARIO_CLASS_PROJECTILE | EE_SCENARIO_CLASS_LAND_BUILDING) == 0
            && air & EE_SCENARIO_CLASS_ATTACKS != 0
            && air & EE_SCENARIO_CLASS_MOVES != 0
        {
            w(h, EE_SCENARIO_WORLD_AI_FAVORITE, &[p, 0, e.mObject as i64]);
        }
        let land = w(h, EE_SCENARIO_WORLD_CLASS, &[e.mIntValue1 as i64]);
        if land & EE_SCENARIO_CLASS_EXISTS != 0
            && land & EE_SCENARIO_CLASS_SEA == 0
            && (land & EE_SCENARIO_CLASS_AIR == 0 || land & EE_SCENARIO_CLASS_FLIES_OVER != 0)
            && land & (EE_SCENARIO_CLASS_PROJECTILE | EE_SCENARIO_CLASS_LAND_BUILDING) == 0
            && land & EE_SCENARIO_CLASS_MOVES != 0
            && land & EE_SCENARIO_CLASS_NOT_LAND == 0
        {
            w(h, EE_SCENARIO_WORLD_AI_FAVORITE, &[p, 1, e.mIntValue1 as i64]);
        }
        let sea = w(h, EE_SCENARIO_WORLD_CLASS, &[e.mIntValue2 as i64]);
        if sea & EE_SCENARIO_CLASS_EXISTS != 0
            && sea & EE_SCENARIO_CLASS_SEA != 0
            && sea & EE_SCENARIO_CLASS_PROJECTILE == 0
            && sea & EE_SCENARIO_CLASS_MOVES != 0
            && sea & EE_SCENARIO_CLASS_ATTACKS != 0
        {
            w(h, EE_SCENARIO_WORLD_AI_FAVORITE, &[p, 2, e.mIntValue2 as i64]);
        }
    }
}

/// The computer players of the effect's player choice (each tested when the loop reaches it, as the original did:
/// the tests only read).
fn computer_players_lazy<H: Host>(h: &mut H, e: &EeScenarioEffectFields) -> Vec<i64> {
    computer_players(h, e.mPlayer).into_iter().map(|p| p as i64).collect()
}

/// `0.02f`: a zoom step.
const ZOOM_STEP: f32 = 0.02;
/// No point yet (`GE3DPoint(-10000, 0, 0)`).
const NO_POINT: (f32, f32, f32) = (-10000.0, 0.0, 0.0);

/// Script-Camera: along an object's camera-object units, else from the start area or unit to the end area or unit
/// over `mIntValue1`, at zoom level `mIntValue2`.
pub fn script_camera<H: Host>(h: &mut H, e: &EeScenarioEffectFields) {
    let exists_object = |h: &mut H, o: c_ulong| h.read(crate::host::EE_SCENARIO_READ_OBJECT_EXISTS, o as i32, 0) != 0;
    let start_object = exists_object(h, e.mTargetObject);
    let end_object = exists_object(h, e.mLocationObject);
    let duration = e.mIntValue1 as i64;
    let mut zoom = e.mIntValue2 as i64 as i32;
    let start_is_area = e.mUsesTargetArea;
    let flags = e.mIsAttack as i64 | (e.mUsesFavorites as i64) << 1;
    w(h, EE_SCENARIO_WORLD_CAMERA_RESET, &[]);
    if !start_is_area
        && start_object
        && e.mTargetObject != 0
        && !e.mUsesLocationArea
        && end_object
        && e.mLocationObject != 0
        && e.mLocationObject == e.mTargetObject
    {
        evaluate(h, e.mLocationObject);
        let n = count(h, e.mLocationObject);
        let units: Vec<i64> = (0..n).map(|i| unit_at(h, e.mLocationObject, i)).collect();
        let mut all = true;
        for &id in &units {
            if !exists(h, id) || unit(h, id, EE_SCENARIO_UV_TYPE) != EE_SCENARIO_K_TYPE_CAMERA_OBJECT {
                all = false;
            }
        }
        if all {
            w(h, EE_SCENARIO_WORLD_CAMERA_TURN_RESET, &[]);
            if zoom != 0 {
                let level = zoom.wrapping_sub(1) as f32 * ZOOM_STEP - 1.0;
                w(h, EE_SCENARIO_WORLD_CAMERA_ZOOM, &[bits(if level > 0.0 { level } else { 0.0 })]);
            }
            for (j, &id) in units.iter().enumerate() {
                let step = if units.len() == 1 || j != 0 { duration } else { 0 };
                w(h, EE_SCENARIO_WORLD_CAMERA_KEY, &[id, step]);
            }
            w(h, EE_SCENARIO_WORLD_CAMERA_START, &[]);
            return;
        }
    }
    let mut end_unit = -1;
    let mut start_unit = -1;
    let mut end = NO_POINT;
    let mut start = NO_POINT;
    let first_unit = |h: &mut H, o: c_ulong| -> i64 {
        evaluate(h, o);
        if count(h, o) != 0 {
            let id = unit_at(h, o, 0);
            if exists(h, id) {
                return id;
            }
        }
        -1
    };
    if e.mUsesLocationArea {
        if let Some(p) = area_point(h, e.mLocationArea) {
            end = p;
        }
    } else if end_object && e.mLocationObject != 0 {
        end_unit = first_unit(h, e.mLocationObject);
    }
    if !start_is_area {
        if start_object && e.mTargetObject != 0 {
            start_unit = first_unit(h, e.mTargetObject);
        }
    } else if let Some(p) = area_point(h, e.mTargetArea) {
        start = p;
    }
    let level = if zoom == 0 {
        -1.1f32
    } else {
        zoom = zoom.wrapping_sub(1);
        let level = zoom as f32 * ZOOM_STEP - 1.0;
        w(h, EE_SCENARIO_WORLD_CAMERA_MODE, &[]);
        level
    };
    for (which, p) in [(0, end), (1, start)] {
        w(h, EE_SCENARIO_WORLD_CAMERA_POINT, &[which, bits(p.0), bits(p.1), bits(p.2)]);
    }
    w(h, EE_SCENARIO_WORLD_CAMERA_MOVE, &[end_unit, start_unit, flags, duration, bits(level)]);
}

/// A set, on-map area's centre tile as a camera point (its middle, at the tile's height), `None` for no map tile.
/// (The original read through a missing area; none gives no point.)
fn area_point<H: Host>(h: &mut H, area: c_ulong) -> Option<(f32, f32, f32)> {
    if !area_usable(h, area) {
        return None;
    }
    let (x, y) = unpack(w(h, EE_SCENARIO_WORLD_AREA_CENTER, &[area as i64]));
    let height = w(h, EE_SCENARIO_WORLD_TILE_HEIGHT, &[x, y]);
    (height >= 0).then(|| (x as i32 as f32 + 0.5, y as i32 as f32 + 0.5, f32::from_bits(height as u32)))
}

#[cfg(test)]
#[path = "effect_tests.rs"]
mod tests;
