//! TEST ONLY: the app profile's cheat sweep (EE-SIM --app-sweep at five game minutes; was EEAppCaptureWorld.cpp's
//! `EEAppCaptureCheatSweepImpl`, `kCheatLines` and `CheatEffect`; tools/sim_capture.py).
//!
//! With the cheats on, each chat line of [`CHEAT_LINES`] is parsed (CHET: the line, its code and parameter); an
//! applied one runs for the local player (and another player when the line names one), then CHEF records every
//! player's state a cheat changes: its five resources, instant build and seeing all, its epoch, an FNV-1a of its units'
//! ids, hit points, resource amounts and (a complex unit's) flight time, power and commandable flag, one of its
//! upgrades' improvement levels, and the camera's location. Last, a code applied with the cheats off (it does
//! nothing). The world is read through the views; the cheats' parse and dispatch, the switch, the upgrades' levels
//! and the camera are the C++'s ([`EeAppCheatSweepHost`]).
#![allow(clippy::unnecessary_cast)] // c_long is 32 bits on Windows, 64 on LP64

use std::ffi::c_void;
use std::os::raw::{c_char, c_long};

use ee_sim::app::capture;
use ee_sim::world::{PlayerView, WorldRead, WorldView};
use ee_sim::world_field;

use crate::app::map_capture::{tag, Fnv, Payload};

/// The C++ calls of the sweep.
#[repr(C)]
pub struct EeAppCheatSweepHost {
    pub ctx: *mut c_void,
    /// `gCheatCodes->ParseCheatCode(line)` -> the code and the parameter
    pub parse: unsafe extern "C" fn(*mut c_void, *const c_char, *mut c_long, *mut c_long),
    /// `gCheatCodes->ApplyCheatCode(code, player, parameter)`
    pub apply: unsafe extern "C" fn(*mut c_void, c_long, c_long, c_long),
    /// `gWorld.mCheatsEnabled = on`; the value before
    pub set_enabled: unsafe extern "C" fn(*mut c_void, bool) -> bool,
    /// upgrade `index` of player `player` (an `EEPlayer*`): its 16 improvement levels into `out` -> 1; 0 no upgrade
    /// there; -1 past the last
    pub levels: unsafe extern "C" fn(*mut c_void, u64, u32, *mut i32) -> i32,
    /// the camera's location at its fraction (x, y, z)
    pub camera: unsafe extern "C" fn(*mut c_void, *mut f32),
}

/// One chat line of the sweep: whether it is applied, and another player to apply it for (-1 none).
struct Line {
    text: &'static str,
    apply: bool,
    other: c_long,
}

const fn line(text: &'static str, apply: bool, other: c_long) -> Line {
    Line { text, apply, other }
}

/// The two that end the game ("somebody set up us the bomb", "ahhhcool") are parsed only.
const CHEAT_LINES: [Line; 42] = [
    line("atm", true, 2),
    line("ATM", true, -1),
    line("boston food sucks", true, -1),
    line("you said wood", true, 3),
    line("rock&roll", true, -1),
    line("creatine", true, -1),
    line("all your base are belong to us", true, -1),
    line("bam", true, -1),
    line("bam 3", true, 2),
    line("bam 12", true, -1),
    line("brainstorm", true, -1),
    line("BrainStorm", true, 3),
    line("boston rent", true, -1),
    line("uh, smoke?", true, -1),
    line("slimfast", true, 2),
    line("mine your own business", true, -1),
    line("girlyman", true, -1),
    line("the big dig", true, 3),
    line("columbus", true, -1),
    line("asus drivers", true, -1),
    line("the quotable patella", true, 2),
    line("display cheats", true, -1),
    line("i have the power", true, 2),
    line("coffee train", true, 3),
    line("friendly skies", true, -1),
    line("atm 0", true, -1),
    line("atm 77", true, -1),
    line("goto 40 50", true, -1),
    line("goto 100000 5", true, -1),
    line("goto 12", true, -1),
    line("find 2 5", true, -1),
    line("find 33554440", true, -1),
    line("find", true, -1),
    line("find x y z", true, -1),
    line("nothing here", true, -1),
    line("", true, -1),
    line("a", true, -1),
    line("9", true, -1),
    line("somebody set up us the bomb", false, -1),
    line("ahhhcool", false, -1),
    line("headshot 10", true, -1),
    line("my name is methos", true, -1),
];

/// `kResourceFood` .. `kResourceIron`
const RESOURCES: std::ops::RangeInclusive<usize> = 2..=6;

/// A player's part of CHEF.
fn player_state(h: &EeAppCheatSweepHost, r: &mut Payload, p: &PlayerView<'_>) {
    let caps: [c_long; 7] = world_field!(p.saved(), mResourceCaps);
    for i in RESOURCES {
        r.i32(caps[i] as i32);
    }
    let instant: bool = world_field!(p.fields(), mInstantBuild);
    r.u32(u32::from(instant) | u32::from(p.sees_all()) << 1);
    r.i32(p.tech_tree().map_or(-1, |t| t.current_epoch() as i32));
    let mut units = Fnv::default();
    let mut count = 0u32;
    for u in p.units() {
        count += 1;
        units.i32(u.id() as i32);
        let hit_points: c_long = world_field!(u.fields(), mHitPoints);
        units.bytes(&hit_points.to_le_bytes());
        let amount: c_long = world_field!(u.fields(), mResourceAmount);
        units.i32(amount as i32);
        if let Some(x) = u.complex_unit() {
            let flight: c_long = world_field!(x.complex(), mFlightTime);
            let power: c_long = world_field!(x.complex(), mPower);
            let commandable: bool = world_field!(x.complex(), mIsCommandable);
            units.bytes(&flight.to_le_bytes());
            units.bytes(&power.to_le_bytes());
            units.i32(i32::from(commandable));
        }
    }
    r.u32(count).hash64(units.0);
    let mut levels = Fnv::default();
    let mut values = [0i32; 16];
    let mut index = 0;
    loop {
        // SAFETY: the C++ callback; a buffer of 16
        match unsafe { (h.levels)(h.ctx, p.handle(), index, values.as_mut_ptr()) } {
            -1 => break,
            1 => values.iter().for_each(|v| levels.i32(*v)),
            _ => {}
        }
        index += 1;
    }
    r.hash64(levels.0);
}

/// CHEF: after line `text`, every player's state and the camera.
fn effect(h: &EeAppCheatSweepHost, text: &str) {
    let mut r = Payload::default();
    r.0.extend_from_slice(text.as_bytes());
    r.u32(0);
    {
        // SAFETY: the sweep runs on the simulation's thread between its steps
        let t = unsafe { WorldRead::begin() };
        if let Some(w) = WorldView::global(&t) {
            for i in 0..w.num_players() as usize {
                if let Some(p) = w.player(i) {
                    player_state(h, &mut r, &p);
                }
            }
        }
    }
    let mut camera = [0f32; 3];
    // SAFETY: the C++ callback; a buffer of 3
    unsafe { (h.camera)(h.ctx, camera.as_mut_ptr()) };
    r.f32(camera[0]).f32(camera[1]).f32(camera[2]);
    capture::commit(tag(b"CHEF"), &r.0);
}

/// The local player's id (`gWorld.GetCurrentPlayer()->mPlayerID`).
fn local_player() -> c_long {
    // SAFETY: as effect
    let t = unsafe { WorldRead::begin() };
    WorldView::global(&t).and_then(|w| w.current_player()).map_or(0, |p| p.id() as c_long)
}

/// The sweep (the caller checks there are cheat codes).
///
/// # Safety
/// `host` is live and its callbacks valid; the caller holds the world as the simulation's step does.
#[no_mangle]
pub unsafe extern "C" fn ee_app_capture_cheat_sweep(host: *const EeAppCheatSweepHost) {
    if !capture::ee_app_capture_active() || host.is_null() {
        return;
    }
    // SAFETY: the caller's contract
    let h = unsafe { &*host };
    // SAFETY: the callbacks (here and below)
    let enabled = unsafe { (h.set_enabled)(h.ctx, true) };
    let local = local_player();
    for l in &CHEAT_LINES {
        let text = std::ffi::CString::new(l.text).unwrap_or_default();
        let (mut code, mut parameter): (c_long, c_long) = (0, 0);
        unsafe { (h.parse)(h.ctx, text.as_ptr(), &mut code, &mut parameter) };
        let mut r = Payload::default();
        r.0.extend_from_slice(l.text.as_bytes());
        r.u32(0).i32(code as i32).i32(parameter as i32);
        capture::commit(tag(b"CHET"), &r.0);
        if !l.apply || code == 0 {
            continue;
        }
        unsafe { (h.apply)(h.ctx, code, local, parameter) };
        if l.other >= 0 {
            unsafe { (h.apply)(h.ctx, code, l.other, parameter) };
        }
        effect(h, l.text);
    }
    // a code applied with cheats off does nothing
    unsafe { (h.set_enabled)(h.ctx, false) };
    unsafe { (h.apply)(h.ctx, 5, local, 0) };
    effect(h, "(cheats off) atm");
    unsafe { (h.set_enabled)(h.ctx, enabled) };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_lines() {
        assert_eq!(CHEAT_LINES.iter().filter(|l| !l.apply).count(), 2);
        assert_eq!(CHEAT_LINES.iter().filter(|l| l.other >= 0).count(), 9);
        assert!(CHEAT_LINES.iter().all(|l| !l.text.contains('\0')));
        // without a capture (tests never set EE_SIM_CAPTURE) the sweep does nothing
        unsafe { ee_app_capture_cheat_sweep(std::ptr::null()) };
    }
}
