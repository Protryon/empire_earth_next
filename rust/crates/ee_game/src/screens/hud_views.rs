//! The HUD hosts' plain world reads answered from `ee_sim::world`'s views (the UI thread lane, round 1) instead of
//! their C++ cases: the main panel group's (`EEUIFMGMainGroup.cpp`, `ee_screenscpp_main_group`), the icon bar's and
//! the portrait's (`EEUICUnitIconBar.cpp`, `EEUICPortrait.cpp`: their `RustOp`s), the improvement button's
//! (`EEUIBUpgrade.cpp`), the command buttons' (`EEUIFMGStatusButton.cpp`'s `EEStatusButtonOp`), the main game form's
//! (`EEUIFMainGame.cpp`) and the status box's view of the selected unit (`EEUIFMGStatusBox.cpp`'s
//! `EEStatusBoxView`). Each answer is its C++ case's statement over the views; an op that is not here goes on to the
//! C++ host (`None`).
//!
//! Units come as handles (`EEUnit*` or `EEComplexUnit*`: the same address, the views' unit handle). The C++ ran on a
//! NULL pointer where a view finds none; the views answer 0 there.
//!
//! Threads: the HUD runs on the UI thread, under the world's read lock (`UpdateUI`, the forms' updates) or its write
//! lock (an action); the token takes no lock, so it is right in both.

// (the casts of the records' `long`s are no-ops on Windows and conversions on LP64)
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use ee_sim::world::{ClassView, GroupView, PlayerView, UnitView, WorldRead, WorldView};
use ee_sim::world_field;
use ee_ui::bgobj::EeUiBgCall;
use ee_ui::direct;
use ee_ui::formobj::EeUiFormCall;
use ee_ui::groupobj::EeUiEntityGroupCall;
use ee_ui::managerobj::EeUiManagerCall;
use ee_ui::obj::EeUiOp;

use ee_base::math::types::Ge3DPoint;

use crate::screens::diplomacy::EeScreensDiplomacyPlayer;
use crate::screens::hot_keys::*;
use crate::screens::hud::EeScreensHudUnit;
use crate::screens::icon_bar::*;
use crate::screens::main_form::*;
use crate::screens::main_group::*;
use crate::screens::portrait::*;
use crate::screens::status_button::*;
use crate::screens::upgrade::*;

/// `kUnitCategoryLandCitizen`, `kUnitCategoryNavalCitizen`, `kUnitCategoryGate` (ee/EEUnitClassIDs.h).
const CATEGORY_LAND_CITIZEN: u64 = 6;
const CATEGORY_NAVAL_CITIZEN: u64 = 0x16;
const CATEGORY_GATE: u64 = 0x19;
/// `kUnitTypeAirport`, `kUnitTypeMarketCP`.
const TYPE_AIRPORT: u64 = 0x42;
const TYPE_MARKET_CP: u64 = 0x321;
/// `kTechEpochAtomicWW2` (ee/EETechTreeTechID.h).
const EPOCH_ATOMIC_WW2: i64 = 10;
/// `kResourceFood` (ee/EEResourceType.h): the first of the five resources in `mResourceCaps`.
const RESOURCE_FOOD: usize = 2;

/// The token of one read (the caller holds the world: see the module's threads).
fn token() -> WorldRead {
    // SAFETY: the HUD's hosts run on the UI thread holding the world's read or write lock
    unsafe { WorldRead::begin() }
}

fn world(t: &WorldRead) -> Option<WorldView<'_>> {
    WorldView::global(t)
}

/// `gWorld.GetCurrentPlayer()`.
fn current(t: &WorldRead) -> Option<PlayerView<'_>> {
    world(t)?.current_player()
}

fn current_handle(t: &WorldRead) -> u64 {
    current(t).map_or(0, |p| p.handle())
}

/// The current player's `mSelectedGroup`.
fn group(t: &WorldRead) -> Option<GroupView<'_>> {
    current(t)?.selected_group()
}

/// The unit `handle` (an `EEUnit*` or `EEComplexUnit*` the C++ handed over).
fn unit(t: &WorldRead, handle: i64) -> Option<UnitView<'_>> {
    // SAFETY: the hosts' unit handles are live units (the selection's, the panel's, the bar's)
    unsafe { UnitView::of(t, handle as u64) }
}

/// The unit's `mPlayer == gWorld.GetCurrentPlayer()`.
fn is_local(t: &WorldRead, u: &UnitView<'_>) -> bool {
    u.player().map_or(0, |p| p.handle()) == current_handle(t)
}

/// The unit's class's `mCategory` and `mUnitType` (0 without a class).
fn category(u: &UnitView<'_>) -> u64 {
    u.class().map_or(0, |c| c.category())
}

fn unit_type(u: &UnitView<'_>) -> u64 {
    u.class().map_or(0, |c| c.unit_type())
}

/// The main panel group's host (`ee_screenscpp_main_group`): its world reads.
pub(crate) fn main_group(op: u32, a: i64, b: i64, c: i64) -> Option<i64> {
    let t = token();
    let complex = || unit(&t, a).and_then(|u| u.complex_unit());
    Some(match op {
        EE_SCREENS_MGR_OP_IN_CINEMATIC => world(&t).is_some_and(|w| w.in_cinematic()) as i64,
        EE_SCREENS_MGR_OP_SELECTED_COUNT => group(&t).map_or(0, |g| g.len() as i64),
        EE_SCREENS_MGR_OP_SELECTED_UNIT => group(&t).map_or(0, |g| g.unit_handle(a as usize) as i64),
        EE_SCREENS_MGR_OP_VIEWED_UNIT => current(&t).map_or(0, |p| p.selected_viewed_unit_handle() as i64),
        EE_SCREENS_MGR_OP_UNIT_HP => unit(&t, a).map_or(0, |u| u.hit_points()),
        EE_SCREENS_MGR_OP_UNIT_ID => unit(&t, a).map_or(0, |u| u.id() as i64),
        EE_SCREENS_MGR_OP_UNIT_KIND => {
            let u = unit(&t, a)?;
            (u.complex_unit_handle() != 0) as i64
                | (is_local(&t, &u) as i64) << 1
                | ((category(&u) == CATEGORY_LAND_CITIZEN) as i64) << 2
                | ((unit_type(&u) == TYPE_MARKET_CP) as i64) << 3
        }
        EE_SCREENS_MGR_OP_UNIT_L294 => {
            unit(&t, a).and_then(|u| u.class()).map_or(0, |cl| world_field!(cl.fields(), mL294) as c_long as i64)
        }
        EE_SCREENS_MGR_OP_AIRPORT => complex().is_some_and(|u| unit_type(&u) == TYPE_AIRPORT) as i64,
        EE_SCREENS_MGR_OP_COMPLEX_LOCAL => complex().is_some_and(|u| is_local(&t, &u)) as i64,
        EE_SCREENS_MGR_OP_COMMAND => complex()
            .and_then(|u| u.class())
            .map_or(0, |cl| world_field!(cl.fields(), mCommands).get(b as usize).map_or(0, |&k| k as c_ulong as i64)),
        EE_SCREENS_MGR_OP_BEFORE_WW2 => complex()
            .and_then(|u| u.player())
            .and_then(|p| p.tech_tree())
            .is_some_and(|tree| tree.current_epoch() < EPOCH_ATOMIC_WW2) as i64,
        EE_SCREENS_MGR_OP_ON_MAP_LOCAL => unit(&t, a).is_some_and(|u| u.has_tile_node() && is_local(&t, &u)) as i64,
        EE_SCREENS_MGR_OP_WORKER => complex().is_some_and(|u| {
            let k = category(&u);
            k == CATEGORY_LAND_CITIZEN || k == CATEGORY_NAVAL_CITIZEN
        }) as i64,
        // the production queue (the citizen's build queue stays the C++'s)
        EE_SCREENS_MGR_OP_QUEUE_SIZE if b == 0 => complex().map_or(0, |u| u.build_queue_len() as i64),
        EE_SCREENS_MGR_OP_QUEUE_KEY if b == 0 => {
            complex().and_then(|u| u.build_queue_technology(c as usize)).unwrap_or(0)
        }
        _ => return None,
    })
}

/// The icon bar's own ops (`EEUICUnitIconBar::RustOp`, `op` without `EE_SCREENS_HC_CLASS`): its world reads.
pub(crate) fn icon_bar(op: u32, a: i64, b: i64) -> Option<i64> {
    let t = token();
    Some(match op {
        EE_SCREENS_IB_OP_GROUP_COUNT => group(&t).map_or(-1, |g| g.len() as i64),
        EE_SCREENS_IB_OP_GROUP_UNIT => group(&t).map_or(0, |g| g.unit_handle(a as usize) as i64),
        EE_SCREENS_IB_OP_CARGO_COUNT => unit(&t, a).map_or(0, |u| u.cargo_len() as i64),
        EE_SCREENS_IB_OP_CARGO => unit(&t, a).map_or(0, |u| u.cargo_handle(b as usize) as i64),
        EE_SCREENS_IB_OP_UNIT_ID => unit(&t, a).map_or(0, |u| u.id() as i64),
        EE_SCREENS_IB_OP_UNIT_TYPE => unit(&t, a).map_or(0, |u| unit_type(&u) as i64),
        EE_SCREENS_IB_OP_TILE => unit(&t, a).map_or(0, |u| {
            let (x, y) = u.tile_xy();
            (x as u32) as i64 | y << 32
        }),
        EE_SCREENS_IB_OP_HIT_POINTS => unit(&t, a).map_or(0, |u| u.hit_points()),
        EE_SCREENS_IB_OP_MAX_HIT_POINTS => unit(&t, a).and_then(|u| u.stats()).map_or(0, |s| s.max_hit_points()),
        EE_SCREENS_IB_OP_BUTTON => unit(&t, a).and_then(|u| u.stats()).map_or(0, |s| s.button_handle() as i64),
        _ => return None,
    })
}

/// The portrait's own ops (`EEUICPortrait::RustOp`, `op` without `EE_SCREENS_HC_CLASS`): its reads of the unit `a`
/// (an `EEComplexUnit*`).
pub(crate) fn portrait(op: u32, a: i64) -> Option<i64> {
    let t = token();
    Some(match op {
        EE_SCREENS_PT_OP_LOCAL => unit(&t, a).is_some_and(|u| is_local(&t, &u)) as i64,
        EE_SCREENS_PT_OP_PRODUCTION => {
            unit(&t, a).map_or(0, |u| world_field!(u.complex(), mProductionTime) as c_long as i64)
        }
        EE_SCREENS_PT_OP_RESEARCH => unit(&t, a).map_or(0, |u| u.current_research()),
        _ => return None,
    })
}

/// The improvement button's host (`ee_screenscpp_upgrade`): the selection's reads.
pub(crate) fn upgrade(op: u32) -> Option<i64> {
    let t = token();
    Some(match op {
        EE_SCREENS_UP_OP_HAS_GROUP => group(&t).is_some() as i64,
        EE_SCREENS_UP_OP_FIRST_UNIT => group(&t).map_or(0, |g| g.first_unit_handle() as i64),
        EE_SCREENS_UP_OP_GROUP_COUNT => group(&t).map_or(0, |g| g.len() as i64),
        EE_SCREENS_UP_OP_GROUP_UNIT => group(&t).map_or(0, |g| g.unit_handle(0) as i64),
        _ => return None,
    })
}

/// The command buttons' part of the form's host (`EEStatusButtonOp`): the selection's and the class table's reads.
pub(crate) fn status_button(op: u32, a: i64) -> Option<i64> {
    let t = token();
    let class = || ClassView::by_index(&t, a as usize);
    let selected = || group(&t).and_then(|g| unit(&t, g.unit_handle(a as usize) as i64));
    Some(match op {
        EE_SCREENS_SB_OP_GROUP_COUNT => group(&t).map_or(-1, |g| g.len() as i64),
        EE_SCREENS_SB_OP_UNIT => selected().is_some() as i64,
        EE_SCREENS_SB_OP_UNIT_TYPE => selected().map_or(0, |u| unit_type(&u) as i64),
        EE_SCREENS_SB_OP_CLASS_PRESENT => class().is_some() as i64,
        EE_SCREENS_SB_OP_CLASS_GATE => class().is_some_and(|c| c.category() == CATEGORY_GATE) as i64,
        EE_SCREENS_SB_OP_CLASS_NAME_ID => {
            class().map_or(0, |c| world_field!(c.fields(), mNameStringID) as c_long as i64)
        }
        _ => return None,
    })
}

/// The main game form's host (`EEUIFMainGame::HostOp`): its reads of the world and the local player.
pub(crate) fn main_form(op: u32, a: i64) -> Option<i64> {
    let t = token();
    Some(match op {
        EE_SCREENS_MF_OP_HAS_PLAYER => current(&t).is_some() as i64,
        EE_SCREENS_MF_OP_GROUP_HAS_FIRST => group(&t).is_some_and(|g| g.first_unit_handle() != 0) as i64,
        EE_SCREENS_MF_OP_HAS_VIEWED => current(&t).is_some_and(|p| p.selected_viewed_unit_handle() != 0) as i64,
        EE_SCREENS_MF_OP_RESOURCE => current(&t)
            .map_or(0, |p| world_field!(p.saved(), mResourceCaps)[(a + 2).clamp(0, 6) as usize] as c_long as i64),
        EE_SCREENS_MF_OP_VICTORY_ENABLED => world(&t).is_some_and(|w| w.victory_enabled()) as i64,
        EE_SCREENS_MF_OP_IN_CINEMATIC => world(&t).is_some_and(|w| w.in_cinematic()) as i64,
        EE_SCREENS_MF_OP_GAME_RESULT => {
            current(&t).map_or(0, |p| world_field!(p.saved(), mGameResult) as c_ulong as i64)
        }
        _ => return None,
    })
}

/// `ee_screenscpp_hot_key_units`' snapshot: the current player's units (its `mFirstUnit` list) as the hot keys see
/// them, the aircraft's (`aircraft`) with the squared distances from `target` (the map point under the view's
/// centre, z 0); `complexes` gets each unit's complex unit handle (the C++'s `sHotKeyUnits`, for `HK_CENTER`).
pub(crate) fn hot_key_units(aircraft: bool, target: [f32; 2], complexes: &mut Vec<u64>) -> Vec<EeScreensHotKeyUnit> {
    let t = token();
    let mut out = Vec::new();
    complexes.clear();
    let Some(player) = current(&t) else { return out };
    let group = player.selected_group();
    let target = Ge3DPoint { x: target[0], y: target[1], z: 0.0 };
    for u in player.units() {
        let mut r = EeScreensHotKeyUnit { unit_id: u.id(), ..Default::default() };
        complexes.push(u.complex_unit_handle());
        out.push(r);
        let Some(c) = u.complex_unit() else { continue };
        r.flags |= EE_SCREENS_HK_F_COMPLEX;
        if u.hit_points() > 0 {
            r.flags |= EE_SCREENS_HK_F_ALIVE;
        }
        if u.tile().is_some() {
            r.flags |= EE_SCREENS_HK_F_ON_TILE;
        }
        if r.flags & (EE_SCREENS_HK_F_ALIVE | EE_SCREENS_HK_F_ON_TILE)
            == EE_SCREENS_HK_F_ALIVE | EE_SCREENS_HK_F_ON_TILE
        {
            // the unit selection's fields
            r.complex_id = c.id();
            r.unit_type = unit_type(&u) as i32;
            r.category = category(&u) as u32;
            r.command = c.command_type();
            r.max_velocity = u.stats().map_or(0, |s| world_field!(s.fields(), mMaxVelocity).to_bits());
        }
        if group.is_some_and(|g| g.contains(c.handle())) {
            r.flags |= EE_SCREENS_HK_F_SELECTED;
        }
        // the aircraft's fields (of the complex unit)
        r.c_category = category(&c) as u32;
        r.c_hit_points = c.hit_points() as i32;
        if c.commandable() {
            r.flags |= EE_SCREENS_HK_F_C_COMMANDABLE;
        }
        if c.tile_xy().0 != -10000 {
            r.flags |= EE_SCREENS_HK_F_C_TILE;
        }
        if c.container_handle() != 0 {
            r.flags |= EE_SCREENS_HK_F_C_CONTAINER;
        }
        if world_field!(c.complex(), mIsDisabled) {
            r.flags |= EE_SCREENS_HK_F_C_DISABLED;
        }
        if c.previous_part_handle() != 0 {
            r.flags |= EE_SCREENS_HK_F_C_PREVIOUS_PART;
        }
        if let Some((kind, _)) = c.goal() {
            r.flags |= EE_SCREENS_HK_F_C_GOAL;
            r.c_goal_kind = kind as u32;
        }
        let cc = c.class();
        r.c_family = cc.map_or(0, |k| k.family() as u32);
        r.c_ammo = cc.map_or(0, |k| world_field!(k.fields(), mAmmoType) as u32);
        r.c_type = cc.map_or(0, |k| k.unit_type() as i32);
        r.c_max_hit_points = c.stats().map_or(0, |s| s.max_hit_points() as i32);
        if aircraft {
            let at = c.container().unwrap_or(c).location();
            r.distance = Ge3DPoint { x: at[0], y: at[1], z: 0.0 }.distance_to_squared(target).to_bits();
        }
        *out.last_mut().unwrap() = r;
    }
    out
}

/// `ee_screenscpp_diplomacy_player`'s fill: player `index` of the world as the diplomacy form reads it (`human`:
/// `IsHuman()`, a virtual the C++ answers); None for an empty entry.
pub(crate) fn diplomacy_player(index: u32, human: impl FnOnce() -> bool) -> Option<EeScreensDiplomacyPlayer> {
    let t = token();
    let p = world(&t)?.player(index as usize)?;
    let saved = p.saved();
    let caps: [c_long; 7] = world_field!(saved, mResourceCaps);
    let enemies: [c_ulong; 17] = world_field!(saved, mEnemies);
    Some(EeScreensDiplomacyPlayer {
        player_id: p.id(),
        game_result: world_field!(saved, mGameResult) as u32,
        observer: p.observer(),
        human: human(),
        slot: p.slot(),
        resources: std::array::from_fn(|r| caps[RESOURCE_FOOD + r] as i32),
        enemies: enemies.map(|e| e as u32),
        send_chat: world_field!(saved, mSendChatMessage),
        tribute_fee: world_field!(p.fields(), mTributeFee),
    })
}

/// The read view's `PLAYER` group's airport question: the player `index`'s selected group holds an airport.
pub(crate) fn airport_selected(index: Option<usize>) -> bool {
    let t = token();
    let Some(w) = world(&t) else { return false };
    let p = match index {
        Some(i) if i < 17 => w.player(i),
        Some(_) => None,
        None => w.current_player(),
    };
    let Some(g) = p.and_then(|p| p.selected_group()) else { return false };
    (0..g.len()).any(|i| unit(&t, g.unit_handle(i) as i64).is_some_and(|u| unit_type(&u) == TYPE_AIRPORT))
}

/// The control `index` of the form `form` (an `EEUIFMainGame*`; NULL when it has none).
#[cfg_attr(test, allow(dead_code))]
fn control(form: direct::Cpp, index: i64) -> direct::Cpp {
    direct::control(form, index as c_ulong)
}

/// The control's `SetDisplayMode(mode)` (a virtual: through the control's host).
#[cfg_attr(test, allow(dead_code))]
fn set_mode(form: direct::Cpp, index: i64, mode: i64) {
    direct::control_virtual(control(form, index), EeUiOp::SetDisplayMode, mode as isize, 0, &mut [0.0; 4]);
}

/// The control's `GetDisplayMode()` (0 for a missing control, where the C++ ran on NULL).
#[cfg_attr(test, allow(dead_code))]
fn mode(form: direct::Cpp, index: i64) -> i64 {
    direct::with_fields(control(form, index), |r| r.mDisplayMode as i64).unwrap_or(0)
}

/// The main game form `form`'s toolkit calls of its host (`EEUIFMainGame::HostOp`), on the toolkit's Rust objects.
#[cfg_attr(test, allow(dead_code))]
pub(crate) fn main_form_toolkit(form: direct::Cpp, op: u32, a: i64, b: i64) -> Option<i64> {
    Some(match op {
        EE_SCREENS_MF_OP_SET_MODE => {
            set_mode(form, a, b);
            0
        }
        EE_SCREENS_MF_OP_MODE => mode(form, a),
        EE_SCREENS_MF_OP_SET_DIRTY => {
            direct::control_virtual(control(form, a), EeUiOp::SetDirty, 1, 0, &mut [0.0; 4]);
            0
        }
        EE_SCREENS_MF_OP_IS_CURRENT => {
            let current = direct::manager_call(direct::manager_of(form), EeUiManagerCall::CurrentForm, 0, 0, 0);
            (current as direct::Cpp == form) as i64
        }
        _ => return None,
    })
}

/// The main panel group's toolkit calls: `owner` its form (`mOwner`), `main` the UI manager's main game form
/// (`gUIManager->mMainGameForm`, whose panels `PANELS_OFF` closes), each asked only when the op needs it.
#[cfg_attr(test, allow(dead_code))]
pub(crate) fn main_group_toolkit(
    owner: impl FnOnce() -> direct::Cpp,
    main: impl FnOnce() -> direct::Cpp,
    op: u32,
    a: i64,
    b: i64,
) -> Option<i64> {
    match op {
        EE_SCREENS_MGR_OP_SET_MODE => set_mode(owner(), a, b),
        EE_SCREENS_MGR_OP_PANELS_OFF => {
            // the behaviours' (9, background 0x97) and the formations' (8, 0x98) panels closed
            let form = main();
            for (group, bg) in [(9, 0x97), (8, 0x98)] {
                let g = direct::form_call(form, EeUiFormCall::EntityGroup, group, 0) as direct::Cpp;
                direct::group_call(g, EeUiEntityGroupCall::Disable, 0);
                let b = direct::form_call(form, EeUiFormCall::BackgroundById, bg, 0) as direct::Cpp;
                direct::background_call(b, EeUiBgCall::SetVisibility, 0, 0);
            }
        }
        EE_SCREENS_MGR_OP_REMOVE_HOT_KEYS => {
            direct::form_call(owner(), EeUiFormCall::RemoveAllHotKeysForAGroup, a as isize, 0);
        }
        _ => return None,
    }
    Some(0)
}

/// `EEStatusBoxView`: the unit `handle`'s values the status box shows (`two_bars`: the box has its third and fourth
/// bars; `fill`: the owner's friendliness is read, only when the class carries or garrisons).
pub(crate) fn status_box_view(handle: i64, two_bars: bool, fill: bool) -> EeScreensHudUnit {
    let t = token();
    let mut v = EeScreensHudUnit::default();
    let Some(u) = unit(&t, handle) else { return v };
    let (Some(class), Some(stats)) = (u.class(), u.stats()) else { return v };
    let sc = stats.class();
    let c = class.fields();
    let s = stats.fields();
    macro_rules! cf {
        ($f:ident) => {
            world_field!(c, $f)
        };
    }
    macro_rules! sf {
        ($f:ident) => {
            world_field!(s, $f)
        };
    }
    macro_rules! scf {
        ($f:ident, $d:expr) => {
            sc.map_or($d, |x| world_field!(x.fields(), $f))
        };
    }
    v.category = cf!(mCategory);
    v.unit_type = cf!(mUnitType);
    v.class_hit_points = cf!(mHitPoints);
    v.hit_points = world_field!(u.fields(), mHitPoints);
    v.max_hit_points = sf!(mMaxHitPoints);
    v.stats_class_hit_points = scf!(mHitPoints, 0);
    v.class_attack = cf!(mAttack);
    v.stats_attack = sf!(mAttack);
    v.stats_class_attack = scf!(mAttack, 0);
    v.class_range = cf!(mRange);
    v.stats_range = sf!(mRange);
    v.stats_class_range = scf!(mRange, 0.0);
    v.class_speed = cf!(mMaxVelocity);
    v.stats_speed = sf!(mMaxVelocity);
    v.stats_class_speed = scf!(mMaxVelocity, 0.0);
    v.class_area_effect = cf!(mAreaEffect);
    v.stats_area_effect = sf!(mAreaEffect);
    v.stats_class_area_effect = scf!(mAreaEffect, 0.0);
    v.class_repair = cf!(mRepairRate);
    v.stats_repair = sf!(mRepairRate);
    v.stats_class_repair = scf!(mRepairRate, 0);
    v.upgrades_range = cf!(mUpgradesRange);
    v.upgrades_area_effect = cf!(mUpgradesAreaEffect);
    v.upgrades_repair = cf!(mUpgradesRepair);
    v.upgrades_armor = [
        cf!(mUpgradesShockArmor),
        cf!(mUpgradesPierceArmor),
        cf!(mUpgradesArrowArmor),
        cf!(mUpgradesGunArmor),
        cf!(mUpgradesMissileArmor),
        cf!(mUpgradesLaserArmor),
    ];
    v.class_transport = cf!(mTransportCapacity);
    v.class_flight_time = cf!(mFlightTime);
    v.class_garrison = cf!(mGarrisonCapacity);
    if fill && (v.class_transport > 0 || v.class_garrison > 0) {
        let me = current(&t);
        let owner = u.player();
        v.friendly = owner.map_or(0, |p| p.handle()) == me.map_or(0, |p| p.handle())
            || match (owner, me) {
                (Some(o), Some(m)) => o.enemy(m.id() as usize) == 0,
                _ => false,
            };
    }
    if let Some(cu) = u.complex_unit() {
        let cc = cu.class();
        let x = cu.complex();
        v.complex = true;
        v.complex_class_transport = cc.map_or(0, |k| world_field!(k.fields(), mTransportCapacity));
        v.complex_class_garrison = cc.map_or(0, |k| world_field!(k.fields(), mGarrisonCapacity));
        v.cargo_room = world_field!(x, mCargoRoom);
        v.flight_time = world_field!(x, mFlightTime);
        v.stats_flight_time = sf!(mFlightTime);
        v.stats_class_flight_time = scf!(mFlightTime, 0);
        v.power = world_field!(x, mPower);
        v.stats_power = sf!(mPower);
        v.stats_class_power = scf!(mPower, 0);
        v.garrison_count = world_field!(x, mGarrisonCount);
    }
    v.two_bars = two_bars;
    v.resource_type = world_field!(u.fields(), mResourceType);
    v.resource_amount = world_field!(u.fields(), mResourceAmount);
    v.carry_amount = sf!(mCarryAmount);
    v.using_carbon = world(&t).is_some_and(|w| w.using_carbon());
    v
}

#[cfg(test)]
#[path = "hud_views_tests.rs"]
mod tests;
