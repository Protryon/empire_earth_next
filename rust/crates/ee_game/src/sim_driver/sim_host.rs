//! TEST ONLY: EE-SIM's host reads and drives that were EESim.cpp's bodies (the C++ answers each call through
//! [`super::sim_ops`]): the UI's controls under the UI lock (the form on top, its class, a click, the lobby's combos,
//! the screens' lists, the control of a screen event), the physics' clock, the editor's presence, the idle human's
//! set-up, the save request, the platform switches, --flare and --volcano, the rules' overrides, the AI manifest's
//! ids, a unit class's value, and the simulation capture's sweeps (the five minutes' and the end's: the combat
//! formulas', the unit-list target search's, the missiles', EEMap's sight).
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use crate::gamecpp as gc;
use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::types::GeTransformation;
use ee_sim::unit_ids::{EEUnitCategory, EEUnitTypeID};

use crate::screens::ui_ids::{EELoadGameControl, EESinglePlayerLoadGameControl};

use super::host::*;
use super::sim_ops::{h, hd, hf, ht, EeSimDriverSim as O, UiLock};

/// cbindgen:ignore
mod sys {
    // the sweeps' Rust (ee_sim units pick_sweep.rs, ag_sweep.rs, unit_sweep.rs, netmsg decisions.rs) and the AI
    // manifest (cp_engine.rs)
    extern "C" {
        pub fn ee_units_select_pick_sweep();
        pub fn ee_units_fill_sweep();
        pub fn ee_units_ag_sweep();
        pub fn ee_units_unit_sweep(call: i64);
        pub fn ee_netmsg_capture_decision_sweep(rates: *const f32);
        pub fn ee_cp_engine_ai_count() -> usize;
        // the combat formulas' folded records (ee_sim combat capture_fold.rs)
        pub fn ee_combat_capture_fold_flush();
    }
}

use crate::gamecpp::{calamity_capture, cheats, msg_capture, stats_capture, tech_capture, weather};
use ee_sim::app::capture as cap;

/// `kSightSnapshot` (ee/EESightCapture.h): the minute's whole-map sight record.
const SIGHT_SNAPSHOT: u32 = 19;

/// `EESimCaptureRecord::Only(tag)` (0 clears it).
fn capture_only(tag: i64) {
    let tag = tag as u32;
    // SAFETY: one tag
    unsafe { cap::ee_app_capture_only(&tag, (tag != 0) as u32) };
}

/// `EECombatCaptureFlush`.
fn combat_flush() {
    // SAFETY: the folders' partial batches written
    unsafe { sys::ee_combat_capture_fold_flush() };
}

const TRIGGER_EVENT: i64 = 0x3f0; // kControlTriggerButtonUp
const FORM_LOBBY: c_ulong = 26;
const FORM_LOAD: c_ulong = 13;
const FORM_LOAD_SCENARIO: c_ulong = 4;
const FORM_CAMPAIGN_SELECT: c_ulong = 33;
const CAMPAIGN_SCENARIO_LIST: c_ulong = 2; // the campaign scenario select form's scenario list

fn form_id(form: usize) -> c_ulong {
    h(O::FormId, form, &[]) as c_ulong
}

fn control_count(form: usize) -> c_ulong {
    h(O::FormControlCount, form, &[]) as c_ulong
}

fn control(form: usize, k: c_ulong) -> usize {
    h(O::FormControl, form, &[k as i64]) as usize
}

fn lines(list: usize) -> Vec<String> {
    (0..h(O::ListLineCount, list, &[])).map(|k| ht(O::ListData, list, k)).collect()
}

/// The form on top's id (0: none).
pub fn top_form() -> u32 {
    if !crate::gamecpp::globals::have!(gUIManager) {
        return 0;
    }
    let _lock = UiLock::hold();
    let form = gc::h(gc::O::SsCurrentForm, 0, &[]) as usize;
    if form != 0 {
        form_id(form) as u32
    } else {
        0
    }
}

/// The form on top's class name (a form shell: the class its kind replaces; a message box is EEUIFError).
pub fn top_form_class() -> String {
    let mut class = "none".to_string();
    if crate::gamecpp::globals::have!(gUIManager) {
        let _lock = UiLock::hold();
        let form = gc::h(gc::O::SsCurrentForm, 0, &[]) as usize;
        let shell = if form != 0 { h(O::FormShellOf, form, &[]) as usize } else { 0 };
        if shell != 0 {
            class = ht(O::ShellKindName, shell, 0);
        } else if form != 0 {
            class = ht(O::FormTypeName, form, 0);
        }
    }
    class
}

/// The control's trigger fired: 0, 1 the form is not on top or has no such control, 2 the control has no trigger.
pub fn click(form: u32, index: u32) -> i32 {
    let _lock = UiLock::hold();
    let top = gc::h(gc::O::SsCurrentForm, 0, &[]) as usize;
    if top == 0 || form_id(top) != form as c_ulong || index as c_ulong >= control_count(top) {
        return 1;
    }
    let c = control(top, index as c_ulong);
    if c == 0 || h(O::ControlTriggerEvent, c, &[TRIGGER_EVENT]) == 0 {
        return 2;
    }
    h(O::ControlFire, c, &[TRIGGER_EVENT]);
    0
}

fn combo(form: usize, index: c_long) -> usize {
    if form == 0 || index < 0 || index as c_ulong >= control_count(form) {
        return 0;
    }
    h(O::ComboOf, control(form, index as c_ulong), &[]) as usize
}

/// The lobby combo's lines and selection; None when the control is not a combo on the lobby.
pub fn combo_lines(index: c_long) -> Option<(Vec<String>, c_long)> {
    let _lock = UiLock::hold();
    let form = gc::h(gc::O::SsCurrentForm, 0, &[]) as usize;
    if form == 0 || form_id(form) != FORM_LOBBY {
        return None;
    }
    let c = combo(form, index);
    let list = if c != 0 { h(O::ComboList, c, &[]) as usize } else { 0 };
    if list == 0 {
        return None;
    }
    let lines = lines(list);
    let mut selection: c_ulong = 0xffff_ffff;
    h(O::ComboSelection, c, &[&mut selection as *mut c_ulong as i64]);
    Some((lines, selection as u32 as i32 as c_long))
}

/// The lobby combo's selection set; false when the lobby went away.
pub fn combo_select(index: c_long, line: c_ulong) -> bool {
    let _lock = UiLock::hold();
    let c = combo(gc::h(gc::O::SsCurrentForm, 0, &[]) as usize, index);
    if c == 0 {
        return false;
    }
    h(O::ComboSetSelection, c, &[line as i64]);
    true
}

/// The load screen's, the play scenario screen's or the campaign scenario select form's file list (0: not on top).
fn screen_list(which: i32) -> usize {
    let form = gc::h(gc::O::SsCurrentForm, 0, &[]) as usize;
    let forms = [FORM_LOAD, FORM_LOAD_SCENARIO, FORM_CAMPAIGN_SELECT];
    let controls = [
        EESinglePlayerLoadGameControl::kSPLoadGameFileListBox as c_ulong,
        EELoadGameControl::kLoadGameLoadFileListBox as c_ulong,
        CAMPAIGN_SCENARIO_LIST,
    ];
    if form == 0 || !(0..=2).contains(&which) {
        return 0;
    }
    let w = which as usize;
    if form_id(form) != forms[w] || controls[w] >= control_count(form) {
        return 0;
    }
    h(O::ListOf, control(form, controls[w]), &[]) as usize
}

/// A screen's list's lines and selection (the campaign's list: the selection only when the list gives one; the file
/// lists: as it is written); None when it is not on top.
pub fn list_lines(which: i32) -> Option<(Vec<String>, c_ulong)> {
    let _lock = UiLock::hold();
    let list = screen_list(which);
    if list == 0 {
        return None;
    }
    let mut selection: c_ulong = 0xffff_ffff;
    let ok = h(O::ListSelection, list, &[&mut selection as *mut c_ulong as i64]) == 0;
    let selection = if which == EE_SIM_DRIVER_LIST_CAMPAIGN && !ok { 0xffff_ffff } else { selection };
    Some((lines(list), selection))
}

/// A screen's list's selection changed (as its keys do); false when it is not on top.
pub fn list_select(which: i32, line: c_ulong) -> bool {
    let _lock = UiLock::hold();
    let list = screen_list(which);
    if list == 0 {
        return false;
    }
    h(O::ListChangeSelection, list, &[line as i64, 1, 0]);
    true
}

/// The control of the form on top whose trigger is the screen event `code` (-1: none, or another form).
pub fn control_with_event(form: u32, code: u32) -> c_long {
    let _lock = UiLock::hold();
    let top = gc::h(gc::O::SsCurrentForm, 0, &[]) as usize;
    if top == 0 || form_id(top) != form as c_ulong {
        return -1;
    }
    for k in 0..control_count(top) {
        let c = control(top, k);
        let event = if c != 0 {
            h(O::ScreenEventOf, h(O::ControlTriggerEvent, c, &[TRIGGER_EVENT]) as usize, &[]) as usize
        } else {
            0
        };
        if event != 0 && h(O::ScreenEventCode, event, &[]) as u32 == code {
            return k as c_long;
        }
    }
    -1
}

/// The physics' clock and state.
pub fn physics() -> EeSimDriverPhysics {
    let mut p = EeSimDriverPhysics { present: gc::h(gc::O::RcPhysicsPresent, 0, &[]) != 0, ..Default::default() };
    if !p.present {
        return p;
    }
    p.timer_running = gc::h(gc::O::SsTimerRunning, 0, &[]) != 0;
    p.game_over = gc::h(gc::O::FwGameOver, 0, &[]) != 0;
    p.game_ms = gc::h(gc::O::DwGameTime, 0, &[]) as c_long;
    p.pt = physics_record().mNumTicksDone;
    p
}

/// The scenario editor on top with a map.
pub fn editor_up() -> bool {
    let _lock = UiLock::hold();
    let editor = h(O::EditorForm, 0, &[]);
    editor != 0 && gc::h(gc::O::SsCurrentForm, 0, &[]) == editor && gc::h(gc::O::EcHasMap, 0, &[]) != 0
}

/// The idle human as the harness's civilization pre-empt leaves it (ai_game.py: the civilization builder opened and
/// cancelled): the civilization timer sends its messages but opens no form, and the computers' civilizations are not
/// locked at its expiry. With `human_out`, allied both ways with every computer from the start: the computers'
/// attack choice prefers a human, and the classic's early raids went to this seat, which never fights back (the
/// computers stay at war with each other; an observer seat, or its units removed, ends the single-player game at
/// once).
pub fn prepare_human(human_out: bool) {
    let human = gc::h(gc::O::C11NmCurrent, 0, &[]) as usize;
    h(O::PlayerSetCivilizationChosen, human, &[1]);
    h(O::PlayerSetCivilizationLocked, human, &[1]);
    if !human_out {
        return;
    }
    for i in 1..h(O::NumPlayers, 0, &[]) {
        let other = crate::tech_tree::objects::world_player(i);
        if other == 0 || other == human {
            continue;
        }
        h(O::PlayerSetStance, human, &[other as i64]);
        h(O::PlayerSetStance, other, &[human as i64]);
    }
}

/// A --save-at request, as EEPhysics::AutoSave sends one (but quiet and not an autosave: no autosave is trimmed, the
/// preferences are not written). It runs at the start of the next communications tick that carries it (EEPhysics::
/// Tick: DrainInput executes it, ServeLoadSaveRequests saves), between two communications ticks.
pub fn request_save(name: &str) {
    use crate::gamecpp::mrecord::{new_record, EeGamecppMrValue as V};
    let text = gc::Owned::new(gc::O::StrNew, gc::O::StrFree);
    crate::gamecpp::preferences::set_text(text.0, true, name.as_bytes());
    let tick = gc::h(gc::O::CommCTsProcessed, 0, &[]);
    let time = i64::from(ee_base::time::ee_time_get());
    // load, quiet, tick, time, auto-save, the wait id, the name
    let values = [
        V::integer(0),
        V::integer(1),
        V::integer(tick),
        V::integer(time),
        V::integer(0),
        V::integer(0),
        V::string(text.0),
    ];
    let message = new_record(ee_sim::message_ids::EEMessageID::kMessageLoadOrSaveGame as u8, &values) as usize;
    h(O::SendToAllAndWait, message, &[]);
    h(O::DerefMessage, message, &[]);
}

/// The platform switches (`gPlatSimMode` on). No fixed ports by default: a pair checked free at start could be taken
/// by another process (another sim, a connection's ephemeral port) before the random-map screen's local server bound
/// it; that server then was not published and its first input crashed (NERecordPlayback::Process on its NULL
/// recorder).
pub fn set_switches(s: &EeSimDriverSwitches) {
    use ee_base::plat_core::sim as plat;
    plat::gPlatSimMode.set(1);
    plat::gPlatSimSeedSet.set(s.seed_set); // a loaded game: the file's seed, as the game loads it
    plat::gPlatSimSeed.set(s.seed);
    plat::gPlatSimNoHumanDefeat.set(s.no_human_defeat);
    h(O::SetScenarioDialogs, 0, &[s.scenario_dialogs as i64]);
    // kPlatSimAnyPort (-1): every port the protocol binds 0
    plat::gPlatSimPortOffset.set(if s.port_offset >= 0 { s.port_offset } else { -1 });
}

/// `gPlatSimNoHumanDefeatHits`; a negative argument reads, 0 clears.
pub fn no_human_defeat_hits(op: c_long) -> c_long {
    use ee_base::plat_core::sim::gPlatSimNoHumanDefeatHits as hits;
    if op == 0 {
        hits.set(0);
    }
    hits.get()
}

// the world constants the sweeps read (include/ee/EEWorldConstants.h: kWorldConversionFaithCost, ...)
pub(super) const WORLD_CONVERSION_FAITH_COST: u32 = 46;
pub(super) const WORLD_WONDER_SIGHT_RADIUS: u32 = 74;
pub(super) const WORLD_TRADE_RATE_STONE_IRON: u32 = 91;
pub(super) const WORLD_TRADE_RATE_FOOD_WOOD: u32 = 92;

/// The unit's Rust record (`static_cast<EeUnitsUnitFields*>`; the live unit's, read on the thread that owns it).
fn unit_fields(u: usize) -> *const ee_sim::world::unit_fields::EeUnitsUnitFields {
    gc::h(gc::O::UnFields, u, &[]) as usize as *const _
}

/// The complex unit's Rust record (`static_cast<EeUnitsComplexUnitFields*>`).
fn cx_fields(c: usize) -> *const ee_sim::world::unit_fields::EeUnitsComplexUnitFields {
    gc::h(gc::O::CxFields, c, &[]) as usize as *const _
}

/// gPhysics' saved block (EE-SIM's reads while the physics lives).
pub(super) fn physics_record() -> ee_sim::units::physics_save::EeUnitsPhysicsSaved {
    let physics = crate::gamecpp::globals::get!(gPhysics) as usize;
    // SAFETY: the live physics' record, copied now
    unsafe { *crate::physics::rec(physics) }
}

/// The complex unit's class category (`mUnitClass->mCategory`).
fn category_of(c: usize) -> i64 {
    // SAFETY: the live unit's record
    let class = unsafe { (*unit_fields(c)).mUnitClass } as usize;
    gc::h(gc::O::RoClassCategory, class, &[])
}

/// `gWorldTable`'s row `id`.
fn world(id: u32) -> ee_data::tables::EeDataWorldEntry {
    ee_data::tables::ee_data_world_entry(id)
}

/// The player's first living building (0: none).
fn first_building(player: c_ulong) -> usize {
    let p = crate::tech_tree::objects::world_player(player as i64);
    let mut u = if p != 0 { gc::h(gc::O::PlFirstUnit, p, &[]) as usize } else { 0 };
    while u != 0 {
        if gc::h(gc::O::UnComplex, u, &[]) != 0
            && gc::h(gc::O::EcHitPoints, u, &[]) as c_long > 0
            && h(O::UnitIsBuilding, u, &[]) != 0
        {
            return u;
        }
        u = gc::h(gc::O::UnNext, u, &[]) as usize;
    }
    0
}

/// --flare: the idle human's flare at the building, told to the human's allies as EEMSendFlare::Execute tells them
/// (EEPlayer::FlareReceived): the AIs' answer to an ally's flare, which no computer sends.
pub fn flare(player: c_ulong) -> EeSimDriverSpot {
    let human = gc::h(gc::O::C11NmCurrent, 0, &[]) as usize;
    let spot = first_building(player);
    if human == 0 || spot == 0 {
        return EeSimDriverSpot::default();
    }
    let (x, y) = (gc::h(gc::O::DkTileX, spot, &[]) as c_long, gc::h(gc::O::DkTileY, spot, &[]) as c_long);
    let allies: Vec<usize> =
        ee_sim::units::player_lists::player_list(human as u64, ee_sim::units::player_lists::EE_UNITS_LISTS_ALLIES)
            .into_iter()
            .map(|a| a as usize)
            .collect();
    for &a in &allies {
        gc::h(gc::O::PlFlareReceived, a, &[human as i64, x as i64, y as i64]);
    }
    EeSimDriverSpot { found: true, x, y, count: allies.len() as c_ulong }
}

/// --volcano: a volcano (the calamity class 9, no caster) cast 6 tiles off the building for that player.
pub fn volcano(player: c_ulong) -> EeSimDriverSpot {
    let spot = first_building(player);
    if spot == 0 {
        return EeSimDriverSpot::default();
    }
    let x = gc::h(gc::O::DkTileX, spot, &[]) as c_long + 6;
    let y = gc::h(gc::O::DkTileY, spot, &[]) as c_long + 6;
    let manager = gc::hp(gc::O::CalamityManager, 0, &[]);
    let cast =
        gc::calamity_objects::ee_gamecpp_calamity_create(manager as *mut c_void, 1, 9, 0xffff_ffff, 0, x, y, player);
    EeSimDriverSpot { found: true, x, y, count: if cast { 1 } else { 0 } }
}

/// The volcano's boulders on the map (every player's volcano projectiles' ids).
pub fn volcano_boulders() -> Vec<c_ulong> {
    let boulder = EEUnitTypeID::kUnitTypeVolcanoProjectile as i64;
    let mut out = Vec::new();
    for i in 0..h(O::NumPlayers, 0, &[]) {
        let p = crate::tech_tree::objects::world_player(i);
        let mut u = if p != 0 { gc::h(gc::O::PlFirstUnit, p, &[]) as usize } else { 0 };
        while u != 0 {
            if h(O::UnitType, u, &[]) == boulder {
                out.push(gc::h(gc::O::RoUnitId, u, &[]) as c_ulong);
            }
            u = gc::h(gc::O::UnNext, u, &[]) as usize;
        }
    }
    out
}

/// A rules override (EE-SIM --set; was EERulesAddOverride): Ok, else the problem.
pub fn rules_add(text: &str) -> Result<(), String> {
    ee_data::rules::add_override(text.as_bytes()).map_err(|e| String::from_utf8_lossy(&e).into_owned())
}

/// The overrides that changed nothing, each with the reason (was EERulesOverrideProblems).
pub fn rules_problems() -> Vec<String> {
    ee_data::rules::override_problems().iter().map(|p| String::from_utf8_lossy(p).into_owned()).collect()
}

/// The world's map and read-write lock (`gWorld.mMap`, `gWorld.mLock`; 0 and null before the world is made, and in a
/// unit test).
pub fn world_map_and_lock() -> (usize, *const ee_base::ts::EeTsLock) {
    #[cfg(not(test))]
    {
        use crate::rts::world::WorldCpp;
        let world = core::ptr::addr_of_mut!(crate::gamecpp::global_objects::gWorld) as usize;
        let mut c = ee_base::shim::ShimFfi;
        // SAFETY: the game's world object's members
        unsafe { (*c.map_slot(world), *c.lock_slot(world) as *const ee_base::ts::EeTsLock) }
    }
    #[cfg(test)]
    (0, core::ptr::null())
}

/// `gWorld.mLock`'s `EE_SIM_DRIVER_*_LOCK` (was EESim.cpp's HostLock): the world's read-write lock, which the world's
/// constructor made.
pub fn world_lock(op: i32) {
    use ee_base::ts::{ee_ts_lock_read, ee_ts_lock_read_unlock, ee_ts_lock_write, ee_ts_lock_write_unlock};
    let lock = world_map_and_lock().1;
    if lock.is_null() {
        return;
    }
    // SAFETY: the world's live lock
    unsafe {
        match op {
            EE_SIM_DRIVER_WRITE_LOCK => ee_ts_lock_write(lock),
            EE_SIM_DRIVER_WRITE_UNLOCK => ee_ts_lock_write_unlock(lock),
            EE_SIM_DRIVER_READ_LOCK => ee_ts_lock_read(lock),
            _ => ee_ts_lock_read_unlock(lock),
        }
    }
}

/// --dump-load: the state dump of `path` at the loaded game's first tick, after the load's own fix-ups (was
/// EESim.cpp's HostDumpAfterLoad: `EEAfterLoad`).
pub fn dump_after_load(path: &str) {
    extern "C" fn run(data: usize) {
        // SAFETY: the box made below, handed over once
        let path = unsafe { Box::from_raw(data as *mut String) };
        super::dump::dump_state(&path);
    }
    gc::persistent::after_load(run, Box::into_raw(Box::new(path.to_string())) as usize);
}

/// The saved game's (0) or the scenario's (1) file extension (`gSavedGameExtension`, `gScenarioExtension`: constants
/// Empire Earth.cpp constructs).
pub fn extension(which: i32) -> &'static str {
    if which == 0 {
        ".ees"
    } else {
        ".scn"
    }
}

/// `gUserInterface->PostCancelGame()` (0x412).
pub fn cancel_game() {
    #[cfg(not(test))]
    crate::screens::post_if_ui(0x412);
}

/// The player's wonder count (`GetNumWonderUnits`: its wonders' list, ee_sim player_lists.rs).
pub fn wonders(player: u64) -> c_long {
    use ee_sim::units::player_lists::{player_op, ListOp, EE_UNITS_LISTS_WONDERS};
    player_op(player, EE_UNITS_LISTS_WONDERS, ListOp::Len) as c_long
}

/// The AI manifest's ids.
pub fn engine_ids() -> Vec<String> {
    (0..unsafe { sys::ee_cp_engine_ai_count() })
        .map(|k| {
            let ai = crate::cp_engine::ee_cp_engine_ai_at(k);
            // SAFETY: the manifest's entry and its id (bytes, no terminator), alive for the program
            unsafe { String::from_utf8_lossy(std::slice::from_raw_parts((*ai).id, (*ai).id_len)).into_owned() }
        })
        .collect()
}

/// A unit class's value in a sample: the cost of the player's tech node of the class's technology, every resource.
pub fn unit_value(player: u64, technology: c_long, unit_type: c_ulong) -> c_long {
    let tree = crate::tech_tree::objects::player_tree(player as usize);
    let node = if tree != 0 { crate::tech_tree::objects::get(tree, technology) } else { 0 };
    let mut value: c_long = 0;
    if node != 0 && !crate::tech_tree::objects::node_data(node).is_null() {
        for r in 2..=6 {
            // kResourceFood .. kResourceIron
            value = value.wrapping_add(crate::tech_tree::objects::ee_gamecpp_tech_obj_cost(
                node as *mut std::ffi::c_void,
                r as std::ffi::c_long,
                unit_type as std::ffi::c_long,
                1.0,
            ));
        }
    }
    value
}

// ------------------------------------------------------------------------------------------------ the sweeps

fn tag(name: &[u8; 4]) -> i64 {
    u32::from_le_bytes(*name) as i64
}

fn capture_only_list(tags: &[i64]) {
    let list: Vec<u32> = tags.iter().map(|&t| t as u32).collect();
    // SAFETY: the list's tags
    unsafe { cap::ee_app_capture_only(list.as_ptr(), list.len() as u32) };
}

/// The combat formulas' sweep's host: EECanHeal, a unit's position, a class's projectile's acceleration.
unsafe extern "C" fn combat_host(_ctx: *mut c_void, op: u32, a: u64, b: u64, out: *mut f32) -> i32 {
    use super::combat_sweep::*;
    match op {
        EE_SIM_DRIVER_COMBAT_CAN_HEAL => {
            h(O::CanHeal, a as usize, &[b as i64]);
            0
        }
        EE_SIM_DRIVER_COMBAT_POSITION => {
            h(O::ComplexPosition, a as usize, &[out as i64]);
            0
        }
        EE_SIM_DRIVER_COMBAT_PROJECTILE => {
            let projectile = h(O::UpgradeProjectile, a as usize, &[]) as usize;
            if projectile == 0 {
                return 0;
            }
            // SAFETY: the sweep's out values (three floats)
            unsafe { *out = hd(O::ClassAcceleration, projectile, &[], &[]) };
            1
        }
        _ => 0,
    }
}

/// The combat formulas' sweep of the simulation capture (combat_sweep.rs, over the world views).
fn combat_capture_sweep() {
    let sweep = super::combat_sweep::EeSimDriverCombatSweep {
        tick_ms: physics_record().mTickTimeMs as i32,
        tick_seconds: physics_record().mTickSeconds,
        regeneration_interval: h(O::RegenerationInterval, 0, &[]) as i32,
        conversion_faith_cost: world(WORLD_CONVERSION_FAITH_COST).mLongValue,
        ctx: std::ptr::null_mut(),
        host: combat_host,
    };
    // SAFETY: the sweep's input, alive through the call
    unsafe { super::combat_sweep::ee_sim_driver_combat_sweep(&sweep) };
}

/// A player's living, armed units on the map in its list order (`priests`: priests kept; `missiles`: missiles and
/// torpedoes kept).
fn armed_units(player: usize, priests: bool, missiles: bool) -> Vec<usize> {
    let (missile, torpedo) = (EEUnitCategory::kUnitCategoryMissile as i64, EEUnitCategory::kUnitCategoryTorpedo as i64);
    let mut all = Vec::new();
    let mut u = if player != 0 { gc::h(gc::O::PlFirstUnit, player, &[]) as usize } else { 0 };
    while u != 0 {
        let c = gc::h(gc::O::UnComplex, u, &[]) as usize;
        if c != 0
            && gc::h(gc::O::EcHitPoints, u, &[]) as c_long > 0
            && !unsafe { (*unit_fields(u)).mStats }.is_null()
            && h(O::ComplexOnMap, c, &[]) != 0
            && gc::h(gc::O::DkTileX, c, &[]) as c_long != -10000
            && hd(O::ComplexAttack, c, &[], &[]) > 0.0
            && (priests || h(O::ComplexIsPriest, c, &[]) == 0)
            && (missiles || ![missile, torpedo].contains(&category_of(c)))
        {
            all.push(c);
        }
        u = gc::h(gc::O::UnNext, u, &[]) as usize;
    }
    all
}

/// The unit-list target search's sweep (CBSL). No game of the capture profiles reaches EESearchTargetAlongList
/// (only anti-air ammunition 0x3c does, through EEFindNearbyTarget), so with EE_SIM_CAPTURE on it runs once, when the
/// game ends, over each player's newest 64 armed units on the map (priests left out: a list unit without a complex
/// unit reaches CanAttackClass through NULL there, as in the original): at the unit's tile, and two tiles off with
/// half the range again. Only its CBSL records are written (not the CBAF, CBLB, CBDP, CBHD records of the tests it
/// calls), and each unit's ignored targets are put back after it (IgnoreTarget, the expired entries' erase): the game
/// is over and no other record follows it.
fn list_search_sweep() {
    capture_only(tag(b"CBSL"));
    for p in 1..h(O::NumPlayers, 0, &[]) {
        let player = crate::tech_tree::objects::world_player(p);
        if player == 0 {
            continue;
        }
        let all = armed_units(player, false, true);
        let first = all.len().saturating_sub(64);
        for &a in &all[first..] {
            let ignored = h(O::IgnoredSave, a, &[]) as usize;
            let (x, y) = (gc::h(gc::O::DkTileX, a, &[]), gc::h(gc::O::DkTileY, a, &[]));
            hf(O::SearchAlongList, a, &[x, y], &[1.0]);
            h(O::IgnoredRestore, a, &[ignored as i64]);
            let (x, y) = (gc::h(gc::O::DkTileX, a, &[]), gc::h(gc::O::DkTileY, a, &[]));
            hf(O::SearchAlongList, a, &[x + 2, y - 2], &[1.5]);
            h(O::IgnoredRestore, a, &[ignored as i64]);
            h(O::IgnoredFree, ignored, &[]);
        }
    }
    capture_only(0);
}

/// The missile sweep (CBMA, CBMH and the impact tests' records). No game of the capture profiles flies a missile
/// (EEPMMissile: the missile and torpedo classes), so with EE_SIM_CAPTURE and --missile-sweep (the capture's missile
/// profile: the oracle's game, its own golden digests of those tags), when the game ends (after the unit-list
/// search's sweep), each player's newest four armed units on the map launch one missile of every missile and torpedo
/// class at the newest armed unit of the next player: the missile is created on the unit's tile 1.5 above it, its
/// launcher class the first class that fires it (else the unit's), alternately at the unit (attack unit) and at its
/// position (attack ground), launched as LaunchShot launches it (LaunchProjectile; a class without a launch delay
/// gets 0.3 s of boost so both steps fly) and put on the map as FlushWorld puts it (the queued placements, the
/// pending units). Then up to 120 ticks of the missile's update without its effects and sound: the physics model's
/// step, the flight time's count down, the water and ground impact tests (the first true ends it). Only the flight
/// steps' and impact tests' records are written, and no other thread's while it runs; the game is over and nothing
/// simulates after it.
fn missile_sweep() {
    capture_only_list(&[tag(b"CBMA"), tag(b"CBMH"), tag(b"CBGI"), tag(b"CBWI")]);
    cap::ee_app_capture_exclusive(true);
    let (missile, torpedo) = (EEUnitCategory::kUnitCategoryMissile as i64, EEUnitCategory::kUnitCategoryTorpedo as i64);
    let table: Vec<usize> = (0..h(O::ClassCount, 0, &[])).map(|k| gc::h(gc::O::UcAt, 0, &[k]) as usize).collect();
    let mut classes = Vec::new();
    let mut launchers = Vec::new();
    for &class in &table {
        if class == 0 {
            continue;
        }
        let category = gc::h(gc::O::RoClassCategory, class, &[]);
        if category != missile && category != torpedo {
            continue;
        }
        let launcher =
            table.iter().copied().find(|&other| other != 0 && h(O::ClassProjectile, other, &[]) as usize == class);
        classes.push(class);
        launchers.push(launcher.unwrap_or(0));
    }
    let mut armed: Vec<Vec<usize>> = Vec::new();
    for p in 1..h(O::NumPlayers, 0, &[]) {
        let all = armed_units(crate::tech_tree::objects::world_player(p), true, false);
        if !all.is_empty() {
            armed.push(all);
        }
    }
    let mut p = 0;
    while armed.len() > 1 && p < armed.len() {
        let target = *armed[(p + 1) % armed.len()].last().unwrap();
        let units = &armed[p];
        let first = units.len().saturating_sub(4);
        for (i, &a) in units.iter().enumerate().skip(first) {
            for (k, &class) in classes.iter().enumerate() {
                if gc::h(gc::O::EcHitPoints, target, &[]) as c_long <= 0
                    || gc::h(gc::O::DkTileX, target, &[]) as c_long == -10000
                {
                    break;
                }
                let owner = gc::h(gc::O::UnPlayer, a, &[]) as usize;
                let created = h(
                    O::ClassCreateUnit,
                    class,
                    &[
                        gc::h(gc::O::DkTileX, a, &[]),
                        gc::h(gc::O::DkTileY, a, &[]),
                        gc::h(gc::O::RoPlayerId, owner, &[]),
                        a as i64,
                    ],
                ) as usize;
                let m = if created != 0 { gc::h(gc::O::UnGetComplex, created, &[]) as usize } else { 0 };
                if m == 0 {
                    continue;
                }
                // SAFETY: the unit's physical model's location (a GETransformation), copied
                let mut start = unsafe { *(h(O::ComplexLocation, a, &[]) as *const GeTransformation) };
                start.m[2][3] += 1.5;
                h(O::ComplexSetLocation, m, &[&start as *const GeTransformation as i64]);
                gc::h(
                    gc::O::CxSetLauncherClass,
                    m,
                    &[
                        if launchers[k] != 0 {
                            launchers[k]
                        } else {
                            (unsafe { (*unit_fields(a)).mUnitClass }) as usize
                        } as i64,
                    ],
                );
                gc::h(gc::O::CxSetOwner, m, &[a as i64]);
                let mut at = [0f32; 3];
                h(O::ComplexPosition, target, &[at.as_mut_ptr() as i64]);
                let goal = if ((i + k) & 1) == 0 {
                    h(O::NewAttackUnit, m, &[target as i64])
                } else {
                    hf(O::NewAttackGround, 0, &[], &at)
                };
                h(O::ComplexSetGoal, m, &[goal]);
                gc::hf(gc::O::CxSetVelocity, m, &[], &[gc::hd(gc::O::EcVelocity, a, &[], &[])]);
                hf(O::ClassLaunchProjectile, class, &[m as i64], &at);
                if unsafe { (*cx_fields(m)).mFlightTime } == 0 {
                    gc::h(gc::O::CxSetFlightTime, m, &[300]);
                }
                // onto the map, as the next tick's EEPhysics::FlushWorld puts a launched shot
                let world = core::ptr::addr_of_mut!(crate::gamecpp::global_objects::gWorld);
                // SAFETY: the live world, under the world lock
                unsafe { super::sed_host::ee_rts_world_flush_queued_placements(world.cast()) };
                h(O::PlayerAddPendingUnits, owner, &[]);
                let mut t = 0;
                while t < 120
                    && gc::h(gc::O::DkTileX, m, &[]) as c_long != -10000
                    && !unsafe { (*cx_fields(m)).mGoalState }.is_null()
                {
                    h(O::ClassStep, class, &[m as i64]);
                    let tick = physics_record().mTickTimeMs;
                    let elapsed = if tick != 0 { tick } else { 1 };
                    let flight = unsafe { (*cx_fields(m)).mFlightTime };
                    if flight != 0 {
                        gc::h(gc::O::CxSetFlightTime, m, &[if flight > elapsed { flight - elapsed } else { 0 } as i64]);
                    }
                    if h(O::ClassWaterImpact, class, &[m as i64]) != 0
                        || h(O::ClassGroundImpact, class, &[m as i64]) != 0
                    {
                        break;
                    }
                    t += 1;
                }
            }
        }
        p += 1;
    }
    cap::ee_app_capture_exclusive(false);
    capture_only(0);
}

/// TSGT, MAPS, MAPK: the app profile's end sweep of EEMap's sight and out-of-synch checksum (ee/EESightCapture.h).
pub fn sight_capture_sweep() {
    if !cap::ee_app_capture_active() || gc::h(gc::O::EcHasMap, 0, &[]) == 0 {
        return;
    }
    let radius = world(WORLD_WONDER_SIGHT_RADIUS).mFloatValue;
    let map_size = gc::h(gc::O::TeMapSize, 0, &[]) as c_long;
    let size = map_size as f32;
    let points = [[size * 0.5, size * 0.5], [5.5, 7.5], [size - 3.25, size * 0.3]];
    let n = gc::h(gc::O::MmNumPlayers, 0, &[]) as c_ulong;
    for p in 1..n {
        let player = gc::h(gc::O::SsWorldPlayer, 0, &[p as i64]) as usize;
        if player == 0 {
            continue;
        }
        for point in &points {
            hf(O::MapAddSight, player, &[0], &[point[0], point[1], radius]);
            hf(O::MapAddSight, player, &[1], &[point[0] + 2.0, point[1] - 1.0, radius * 0.5]);
            hf(O::MapRemoveSight, player, &[0], &[point[0], point[1], radius]);
        }
        hf(O::MapRemoveSight, player, &[1], &[points[0][0] + 2.0, points[0][1] - 1.0, radius * 0.5]);
    }
    if n > 1 {
        let last = gc::h(gc::O::SsWorldPlayer, 0, &[(n - 1) as i64]) as usize;
        gc::h(gc::O::DcExplore, last, &[]);
        h(O::MapFogSeen, last, &[]);
    }
    // two full cycles of the checksum's 64 x 64 windows
    let bands = (map_size + 63) / 64;
    let map = crate::gamecpp::hp(crate::gamecpp::O::MpWorldMap, 0, &[]) as *mut c_void;
    for _ in 0..2 * bands * bands + 1 {
        crate::gamecpp::map::ee_gamecpp_map_checksum(map);
    }
}

/// A sweep or capture hook (`EE_SIM_DRIVER_SWEEP_*`).
pub fn sweep(kind: i32, arg: c_long) {
    match kind {
        EE_SIM_DRIVER_SWEEP_FIVE_MINUTES => {
            // every five game minutes of a captured game
            // SAFETY: the simulation thread holds the world
            unsafe { ee_sim::formations::units::capture_sweep() };
            combat_capture_sweep();
            tech_capture::ee_gamecpp_tech_capture_sweep(); // the tech trees' sweep (ee/EETechCapture.h)
            calamity_capture::ee_gamecpp_capture_calamity_sweep(); // CanCastAt over every calamity class (ee/EETechCapture.h)
                                                                   // the Execute decisions' sweep (ee_sim netmsg decisions.rs): EEGetTradePrice's rates by resource
            let rates = [
                1.0f32,
                1.0,
                world(WORLD_TRADE_RATE_FOOD_WOOD).mFloatValue,
                world(WORLD_TRADE_RATE_FOOD_WOOD).mFloatValue2,
                world(WORLD_TRADE_RATE_STONE_IRON).mFloatValue,
                1.0,
                world(WORLD_TRADE_RATE_STONE_IRON).mFloatValue2,
            ];
            // SAFETY: the seven rates; the sweeps over the world views
            unsafe {
                sys::ee_netmsg_capture_decision_sweep(rates.as_ptr());
                sys::ee_units_select_pick_sweep(); // the box selection's pick over every player's units
                sys::ee_units_fill_sweep(); // the active groups' Fill test over every player's units
            }
            h(O::GroupCommandSweep, 0, &[]); // the group commands' bodies over the dry host (ee/EEUnitHost.h)
        }
        EE_SIM_DRIVER_SWEEP_MESSAGES => {
            msg_capture::ee_gamecpp_msgcap_sweep(); // once (ee/EEMessageCapture.h)
        }
        EE_SIM_DRIVER_SWEEP_CALAMITIES => {
            calamity_capture::ee_gamecpp_capture_cast_calamities(arg); // --calamities
        }
        // SAFETY: the unit sweep over the world views
        EE_SIM_DRIVER_SWEEP_UNITS => unsafe { sys::ee_units_unit_sweep(arg as i64) }, // --unit-sweep
        EE_SIM_DRIVER_SWEEP_MINUTE => {
            // every game minute: the mini map's cells (MMSN; ee_game app world_capture.rs), the tile sight (TSGT)
            if crate::gamecpp::globals::have!(gMiniMap) {
                h(O::MiniMapSnapshot, 0, &[]);
            }
            crate::sight_capture::ee_app_sight_open_whole(SIGHT_SNAPSHOT, std::ptr::null());
            crate::sight_capture::ee_app_sight_close();
        }
        EE_SIM_DRIVER_SWEEP_CHEATS => {
            cheats::ee_gamecpp_capture_cheat_sweep(); // --app-sweep at five game minutes
        }
        EE_SIM_DRIVER_SWEEP_STORMS => {
            weather::ee_gamecpp_capture_storm_sweep(); // --app-sweep at ten
        }
        EE_SIM_DRIVER_SWEEP_END => end_sweep(arg),
        EE_SIM_DRIVER_SWEEP_HOLD => {
            cap::ee_app_capture_hold(arg != 0);
        }
        // --scenario: a shown dialog message ends now (ee/EEScenarioManager.h)
        EE_SIM_DRIVER_SWEEP_DIALOG if h(O::DialogShowing, 0, &[]) != 0 => {
            h(O::ShowDialogMessage, 0, &[]);
        }
        _ => {}
    }
}

/// The end of a game; `arg` 0: nothing swept, only the flush.
fn end_sweep(arg: c_long) {
    if arg & 2 != 0 {
        // the 3D effects' sweep alone (the game's own records held since its start)
        cap::ee_app_capture_hold(false);
        // SAFETY: the game's effect manager (null: none)
        unsafe { ee_render::gfx::effect_sweep::ee_gfx_effects_sweep(crate::gamecpp::fx_world::current()) };
        combat_flush();
        cap::ee_app_capture_close();
        return;
    }
    if arg & 4 != 0 {
        // the active groups' sweep alone (the game's own records held since its start)
        cap::ee_app_capture_hold(false);
        // SAFETY: the active groups' sweep over the world views (ee_sim units ag_sweep.rs)
        unsafe { sys::ee_units_ag_sweep() };
        combat_flush();
        cap::ee_app_capture_close();
        return;
    }
    if arg & 1 != 0 {
        list_search_sweep();
    }
    if arg & 8 != 0 {
        // EEMap's sight and out-of-synch checksum (MAPS, MAPK; their TSGT): no game reaches them. The changed sight
        // would reach the other threads: nothing records after it
        combat_flush();
        capture_only_list(&[tag(b"MAPS"), tag(b"MAPK"), tag(b"TSGT")]);
        cap::ee_app_capture_exclusive(true);
        sight_capture_sweep();
        // EEWorld::RecordStatistics (WSTA): only a game left runs it (EEWorld::Clear); its random draws and the
        // computers' topped-up totals reach nothing after it
        capture_only(tag(b"WSTA"));
        stats_capture::ee_gamecpp_capture_statistics_sweep();
        cap::ee_app_capture_exclusive(false);
        capture_only(0);
        cap::ee_app_capture_close();
    }
    if arg & 16 != 0 {
        missile_sweep();
        // the missiles it made stay on the map (the AI and UI threads would see them): nothing records after it
        combat_flush();
        cap::ee_app_capture_close();
    }
    combat_flush();
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tags_are_the_cpp_packing() {
        // EE_COMBAT_TAG('C', 'B', 'S', 'L'): a | b << 8 | c << 16 | d << 24
        assert_eq!(tag(b"CBSL"), (b'C' as i64) | (b'B' as i64) << 8 | (b'S' as i64) << 16 | (b'L' as i64) << 24);
    }

    #[test]
    fn sight_snapshot_is_the_header_kind() {
        // the sight kinds' enum of ee/EESightCapture.h: kSightTileAdd = 1, then one each
        let header =
            std::fs::read_to_string(concat!(env!("CARGO_MANIFEST_DIR"), "/../../../include/ee/EESightCapture.h"))
                .expect("the header");
        let kinds: Vec<&str> = header
            .lines()
            .map(str::trim)
            .filter(|l| l.starts_with("kSight"))
            .map(|l| l.split(|c: char| !c.is_ascii_alphanumeric()).next().unwrap())
            .collect();
        assert_eq!(kinds[0], "kSightTileAdd");
        assert_eq!(kinds.iter().position(|&k| k == "kSightSnapshot").map(|k| k as u32 + 1), Some(SIGHT_SNAPSHOT));
    }
}
