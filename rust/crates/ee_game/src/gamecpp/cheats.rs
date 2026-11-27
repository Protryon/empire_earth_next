//! The cheat codes' host (game core lane, round 10, lane D; was EECheatCodes.cpp's `EECheatHostOp`, `EECheatAction`
//! and `EECheatKill`, and EEAppCaptureWorld.cpp's cheat sweep host): ee_sim app cheats.rs's [`CheatHost`] over
//! one-statement cases of EECheatCodes.cpp's `EECore10OpD4` ([`super::EeGameCpp`]'s `Dc` ops): the strings, the chat
//! log (and its CHLS record), the camera's goto and find, and each cheat's walk over a player's units.
//!
//! Simulation code (RUST_PLAN 2.1): the walks visit the player's unit list in its order, each unit's next read after
//! its body as the C++ did; the research loop counts down with the C++'s `long`.
#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_char, c_long, c_ulong, c_void, CStr, CString};

use ee_base::math::types::Ge3DPoint;
use ee_sim::app::cheats::{CheatAction, CheatHost, EeAppCheats};
use ee_sim::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use ee_sim::world::unit_class_fields::EeUnitsUnitClassFields;
use ee_sim::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

use crate::app::cheat_capture::{ee_app_capture_cheat_sweep, EeAppCheatSweepHost};
use crate::app::world_capture::ee_app_capture_cheat_list;

use super::{cptr, h, hp, pa, ptr, rec, Owned, O, P};

/// `kUnitCategoryResource`, `kTechEpochPrehistoric`.
const CATEGORY_RESOURCE: c_ulong = 1;
const EPOCH_PREHISTORIC: c_long = 0;

fn uf<'a>(u: P) -> &'a mut EeUnitsUnitFields {
    // SAFETY: a live unit's record, used before the next C++ call
    unsafe { &mut *crate::gamecpp::records::un_fields(u) }
}

fn cf<'a>(cx: P) -> &'a mut EeUnitsComplexUnitFields {
    // SAFETY: as uf
    unsafe { &mut *crate::gamecpp::records::cx_fields(cx) }
}

fn stats<'a>(u: P) -> &'a EeUnitsUnitClassUpgradeFields {
    // SAFETY: the unit's upgrade (mStats), as uf
    unsafe { &*rec(O::DcStats, u) }
}

fn class_of<'a>(u: P) -> &'a EeUnitsUnitClassFields {
    // SAFETY: the unit's class record, as uf
    unsafe { &*crate::gamecpp::records::uc_fields(hp(O::UnClass, u, &[])) }
}

/// The units of a player's list, each one's next read after the body ran.
fn each_unit(pl: P, mut body: impl FnMut(P)) {
    let mut u = hp(O::PlFirstUnit, pl, &[]);
    while u != 0 {
        body(u);
        u = hp(O::UnNext, u, &[]);
    }
}

/// The C++ string's bytes.
fn text(s: &Owned) -> Vec<u8> {
    let t = hp(O::DcStrBytes, s.0, &[]) as *const c_char;
    if t.is_null() {
        return Vec::new();
    }
    // SAFETY: the string's C text, read before the string goes
    unsafe { CStr::from_ptr(t) }.to_bytes().to_vec()
}

/// A C string of the bytes up to the first NUL (what the C++'s `std::string(...).c_str()` passed on).
fn c_line(line: &[u8]) -> CString {
    let end = line.iter().position(|&b| b == 0).unwrap_or(line.len());
    CString::new(&line[..end]).unwrap_or_default()
}

/// `RevealMap`'s kill: every unit of the player (resource units emptied first); not `all`: the local player's
/// viewed unit and its active group's units are spared.
fn kill(pl: P, all: bool) {
    let current = hp(O::WorldCurrentPlayer, 0, &[]);
    each_unit(pl, |u| {
        let cx = hp(O::UnComplex, u, &[]);
        if !all && h(O::DcIsViewed, current, &[pa(u)]) != 0 {
            return;
        }
        if cx != 0 {
            if all || h(O::DcActiveContains, current, &[pa(cx)]) == 0 {
                h(O::DcCxKill, cx, &[]);
            }
        } else {
            if uf(u).mResourceAmount != 0 {
                uf(u).mResourceAmount = 0;
            }
            h(O::UnKill, u, &[0, 0, 0]);
            if class_of(u).mCategory == CATEGORY_RESOURCE {
                h(O::DcPlayerKillUnit, hp(O::UnPlayer, u, &[]), &[pa(u)]);
            }
        }
    });
}

/// `ResearchAll`: every improvement the player can still get, to the class's highest level; the queued ones dropped.
fn research_all(pl: P) {
    let classes = h(O::DcClassCount, 0, &[]) as c_long;
    for i in 0..classes {
        let class = crate::tech_tree::objects::class_at(i as i64);
        if class == 0 {
            continue;
        }
        let upgrade = hp(O::DcPlayerUpgrade, pl, &[i as i64]);
        if upgrade != 0 {
            for j in 0..0x10i64 {
                if h(O::DcCanImprove, pl, &[i as i64, j]) == 0 {
                    continue;
                }
                let max = h(O::DcClassMaxLevel, class, &[]) as c_long;
                let mut n = max.wrapping_sub(h(O::DcUpgradeLevel, upgrade, &[j]) as c_long).wrapping_add(1);
                while n != 0 {
                    h(O::DcApplyImprovement, pl, &[i as i64, j]);
                    n = n.wrapping_sub(1);
                }
            }
        }
        h(O::DcEraseQueued, pl, &[i as i64]);
    }
}

/// One cheat's work for a player (below the player count).
fn action(action: CheatAction, player: c_ulong) {
    let pl = hp(O::WorldPlayer, 0, &[player as i64]);
    match action {
        CheatAction::Kill { all } => kill(pl, all),
        CheatAction::Explore => {
            h(O::DcExplore, pl, &[]);
        }
        CheatAction::MakeVisible => {
            // the message that makes the whole world visible, from the local player only
            if pl == hp(O::WorldCurrentPlayer, 0, &[]) {
                let message =
                    super::mrecord::new_record(ee_sim::message_ids::EEMessageID::kMessageMakeWorldVisible as u8, &[]);
                h(O::DcSetOriginatingSlot, message, &[pa(pl)]);
                h(O::DcProcessInput, message, &[]);
            }
        }
        CheatAction::ResearchAll => research_all(pl),
        CheatAction::AdvanceEpoch(epoch) => {
            if epoch < ee_sim::tech::tech_ids::ee_tech_epoch_count() as c_long
                && epoch > EPOCH_PREHISTORIC
                && epoch
                    > crate::tech_tree::objects::current_epoch(crate::tech_tree::objects::player_tree(pl)) as c_long
            {
                h(O::DcSetEpoch, pl, &[epoch as i64]);
            }
        }
        CheatAction::RestorePower => each_unit(pl, |u| {
            let cx = hp(O::UnComplex, u, &[]);
            if uf(u).mHitPoints > 0 && cx != 0 && cf(cx).mPowerRegenTimer > 0 && cf(cx).mPower != stats(u).mPower {
                cf(cx).mPower = stats(u).mPower;
            }
        }),
        CheatAction::Heal => each_unit(pl, |u| {
            if uf(u).mHitPoints > 0 {
                h(O::DcSetHitPoints, u, &[stats(u).mMaxHitPoints as i64]);
            }
        }),
        CheatAction::Refuel => each_unit(pl, |u| {
            // living aircraft refuelled; grounded ones commandable again
            let cx = hp(O::UnComplex, u, &[]);
            if !(uf(u).mHitPoints > 0) || cx == 0 || !(stats(u).mFlightTime > 0) {
                return;
            }
            let c = cf(cx);
            // the flag's byte as the C++ compared it (a bool other than 0 or 1)
            let commandable = std::ptr::addr_of_mut!(c.mIsCommandable) as *mut u8;
            // SAFETY: the record's byte
            unsafe {
                if c.mFlightTime == 0 && *commandable != 0 && *commandable != 1 {
                    *commandable = 1;
                }
            }
            if c.mFlightTime != stats(u).mFlightTime {
                c.mFlightTime = stats(u).mFlightTime;
            }
        }),
        CheatAction::ToggleInstantBuild => {
            // SAFETY: the player's record
            let f = unsafe { &mut *crate::gamecpp::records::pl_fields(pl) };
            f.mInstantBuild = !f.mInstantBuild;
        }
        CheatAction::EndGame { win } => {
            let local = pl == hp(O::WorldCurrentPlayer, 0, &[]);
            h(O::DcEndGame, 0, &[(if win { local } else { !local }) as i64]);
        }
        CheatAction::AddResource(resource, amount) => {
            h(O::PlAddResource, pl, &[resource as i64, amount as i64]);
        }
        CheatAction::ClearResource(resource) => {
            // SAFETY: the player's record
            let f = unsafe { &mut *crate::gamecpp::records::pl_saved(pl) };
            f.mResourceCaps[resource as usize] = 0;
        }
    }
}

/// The camera's location moved to (x, y) at once, following the terrain.
fn camera_to(x: f32, y: f32) {
    let mut location = super::camera_io::location();
    location.x = x;
    location.y = y;
    super::camera_io::set_location(&location, true, true);
}

/// The host: each C++ fact one case.
struct Host;

impl CheatHost for Host {
    fn string(&mut self, id: u32) -> Vec<u8> {
        let s = Owned::new(O::StrNew, O::StrFree);
        h(O::ResString, s.0, &[id as i64]);
        text(&s)
    }
    fn multiplayer(&mut self) -> bool {
        h(O::DcMultiplayer, 0, &[]) != 0
    }
    fn cheats_enabled(&mut self) -> bool {
        h(O::DcCheatsEnabled, 0, &[]) != 0
    }
    fn players(&mut self) -> c_ulong {
        h(O::DcNumPlayers, 0, &[]) as c_ulong
    }
    fn chat(&mut self, line: &[u8]) {
        let line = c_line(line);
        h(O::DcChatLog, 0, &[line.as_ptr() as usize as i64]);
        // SAFETY: our C string (CHLS, TEST ONLY: ee_game app world_capture.rs)
        unsafe { ee_app_capture_cheat_list(line.as_ptr()) };
    }
    fn goto(&mut self, x: f32, y: f32) {
        // the camera to the map's closest tile of the point
        let point = Ge3DPoint::new(x, y, 0.0);
        let (mut tx, mut ty): (c_long, c_long) = (0, 0);
        if h(O::DcClosestTile, 0, &[cptr(&point), ptr(&mut tx), ptr(&mut ty)]) != 0 {
            camera_to(tx as f32, ty as f32);
        }
    }
    fn find(&mut self, id: c_ulong) {
        // the camera to the unit's tile
        let u = hp(O::DcWorldUnit, 0, &[id as i64]);
        if u != 0 {
            // SAFETY: the unit's RTS record
            let r = unsafe { &*(u as *const EeUnitsRtsUnitFields) };
            camera_to(r.mTileX as f32, r.mTileY as f32);
        }
    }
    fn action(&mut self, action: CheatAction, player: c_ulong) {
        self::action(action, player);
    }
}

/// `EECheatCodes()`: the table from the string resources.
#[no_mangle]
pub extern "C" fn ee_gamecpp_cheats_new() -> *mut EeAppCheats {
    Box::into_raw(Box::new(EeAppCheats::new(&mut Host)))
}

/// `ApplyCheatCode(code, player, parameter)`.
///
/// # Safety
/// `cheats` came from [`ee_gamecpp_cheats_new`] and is live.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_cheats_apply(
    cheats: *const EeAppCheats,
    code: c_long,
    player: c_long,
    parameter: c_long,
) {
    // SAFETY: the function's contract
    unsafe { (*cheats).apply(&mut Host, code, player, parameter) };
}

/// `ParseCheatCode(text)`: `code` and (when the line ends in a number) `parameter`.
///
/// # Safety
/// `cheats` is live, `text` a C string, the outputs live.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_cheats_parse(
    cheats: *const EeAppCheats,
    text: *const c_char,
    code: *mut c_long,
    parameter: *mut c_long,
) {
    // SAFETY: the function's contract
    unsafe {
        let text = if text.is_null() { &[][..] } else { CStr::from_ptr(text).to_bytes() };
        (*cheats).parse(&mut Host, text, &mut *code, &mut *parameter);
    }
}

// ---- the app profile's cheat sweep's C++ calls (TEST ONLY: ee_game app cheat_capture.rs)

unsafe extern "C" fn sweep_parse(_: *mut c_void, line: *const c_char, code: *mut c_long, parameter: *mut c_long) {
    h(O::DcParse, 0, &[line as usize as i64, code as usize as i64, parameter as usize as i64]);
}

unsafe extern "C" fn sweep_apply(_: *mut c_void, code: c_long, player: c_long, parameter: c_long) {
    h(O::DcApply, 0, &[code as i64, player as i64, parameter as i64]);
}

unsafe extern "C" fn sweep_set_enabled(_: *mut c_void, on: bool) -> bool {
    h(O::DcSwapCheatsEnabled, 0, &[on as i64]) != 0
}

unsafe extern "C" fn sweep_levels(_: *mut c_void, player: u64, index: u32, out: *mut i32) -> i32 {
    let pl = player as P;
    if index as i64 >= h(O::DcPlayerUpgradeCount, pl, &[]) {
        return -1;
    }
    let upgrade = hp(O::DcPlayerUpgrade, pl, &[index as i64]);
    if upgrade == 0 {
        return 0;
    }
    for j in 0..0x10 {
        // SAFETY: the sweep's 16 levels
        unsafe { *out.add(j) = h(O::DcUpgradeLevel, upgrade, &[j as i64]) as i32 };
    }
    1
}

unsafe extern "C" fn sweep_camera(_: *mut c_void, out: *mut f32) {
    let location = super::camera_io::location();
    // SAFETY: the sweep's three floats
    unsafe { std::ptr::copy_nonoverlapping([location.x, location.y, location.z].as_ptr(), out, 3) };
}

/// EE-SIM --app-sweep's cheats (TEST ONLY: CHET, CHEF), when there are cheat codes.
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_cheat_sweep() {
    if !ee_sim::app::capture::ee_app_capture_active() || !crate::gamecpp::globals::have!(gCheatCodes) {
        return;
    }
    let host = EeAppCheatSweepHost {
        ctx: std::ptr::null_mut(),
        parse: sweep_parse,
        apply: sweep_apply,
        set_enabled: sweep_set_enabled,
        levels: sweep_levels,
        camera: sweep_camera,
    };
    // SAFETY: our host, live for the call
    unsafe { ee_app_capture_cheat_sweep(&host) };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_chat_line_stops_at_its_first_nul() {
        assert_eq!(c_line(b"bam 3").as_bytes(), b"bam 3");
        assert_eq!(c_line(b"ab\0cd").as_bytes(), b"ab");
        assert_eq!(c_line(b"").as_bytes(), b"");
    }

    #[test]
    #[should_panic(expected = "no C++")]
    fn the_host_is_the_cpp() {
        Host.players();
    }
}
